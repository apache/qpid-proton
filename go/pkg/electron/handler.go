/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

package electron

import (
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/proton"
)

// NOTE: methods in this file are called only in the proton goroutine unless otherwise indicated.

type handler struct {
	delegator  *proton.MessagingAdapter
	connection *connection
	links      map[proton.Link]Endpoint
	sent       map[proton.Delivery]*sendable // Waiting for outcome
	sessions   map[proton.Session]*session
}

func newHandler(c *connection) *handler {
	h := &handler{
		connection: c,
		links:      make(map[proton.Link]Endpoint),
		sent:       make(map[proton.Delivery]*sendable),
		sessions:   make(map[proton.Session]*session),
	}
	h.delegator = proton.NewMessagingAdapter(h)
	// Disable auto features of MessagingAdapter, we do these ourselves.
	h.delegator.Prefetch = 0
	h.delegator.AutoAccept = false
	h.delegator.AutoSettle = false
	h.delegator.AutoOpen = false
	return h
}

func (h *handler) linkError(l proton.Link, msg string) {
	proton.CloseError(l, amqp.Errorf(amqp.InternalError, "%s for %s %s", msg, l.Type(), l))
}

func (h *handler) HandleMessagingEvent(t proton.MessagingEvent, e proton.Event) {
	switch t {

	case proton.MMessage:
		if r, ok := h.links[e.Link()].(*receiver); ok {
			r.message(e.Delivery())
		} else {
			h.linkError(e.Link(), "no receiver")
		}

	case proton.MSettled:
		if sm, ok := h.sent[e.Delivery()]; ok {
			d := e.Delivery().Remote()
			sm.ack <- Outcome{sentStatus(d.Type()), d.Condition().Error(), sm.v}
			delete(h.sent, e.Delivery())
		}

	case proton.MSendable:
		if s, ok := h.links[e.Link()].(*sender); ok {
			s.trySend()
		} else {
			h.linkError(e.Link(), "no sender")
		}

	case proton.MConnectionOpening:
		h.connection.heartbeat = e.Transport().RemoteIdleTimeout()
		if e.Connection().State().LocalUninit() { // Remotely opened
			h.incoming(newIncomingConnection(h.connection))
		}
		h.connection.wakeSync()

	case proton.MSessionOpening:
		if e.Session().State().LocalUninit() { // Remotely opened
			h.incoming(newIncomingSession(h, e.Session()))
		}
		h.sessions[e.Session()].wakeSync()

	case proton.MSessionClosed:
		h.sessionClosed(e.Session(), proton.EndpointError(e.Session()))

	case proton.MLinkOpening:
		l := e.Link()
		if ss := h.sessions[l.Session()]; ss != nil {
			if l.State().LocalUninit() { // Remotely opened.
				if l.IsReceiver() {
					h.incoming(newIncomingReceiver(ss, l))
				} else {
					h.incoming(newIncomingSender(ss, l))
				}
			}
			if ep, ok := h.links[l]; ok {
				ep.(endpointInternal).wakeSync()
			} else {
				h.linkError(l, "no link")
			}
		} else {
			h.linkError(l, "no session")
		}

	case proton.MLinkClosing:
		e.Link().Close()

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), proton.EndpointError(e.Link()))

	case proton.MConnectionClosing:
		h.connection.err.Set(e.Connection().RemoteCondition().Error())

	case proton.MConnectionClosed:
		h.shutdown(proton.EndpointError(e.Connection()))

	case proton.MDisconnected:
		var err error
		if err = e.Connection().RemoteCondition().Error(); err == nil {
			if err = e.Connection().Condition().Error(); err == nil {
				if err = e.Transport().Condition().Error(); err == nil {
					err = amqp.Errorf(amqp.IllegalState, "unexpected disconnect on %s", h.connection)
				}
			}
		}
		h.shutdown(err)
	}
}

func (h *handler) incoming(in Incoming) {
	var err error
	if h.connection.incoming != nil {
		h.connection.incoming <- in
		// Must block until accept/reject, subsequent events may use the incoming endpoint.
		err = in.wait()
	} else {
		err = amqp.Errorf(amqp.NotAllowed, "rejected incoming %s %s",
			in.pEndpoint().Type(), in.pEndpoint().String())
	}
	if err == nil {
		in.pEndpoint().Open()
	} else {
		proton.CloseError(in.pEndpoint(), err)
	}
}

func (h *handler) addLink(pl proton.Link, el Endpoint) {
	h.links[pl] = el
}

func (h *handler) linkClosed(l proton.Link, err error) {
	if link, ok := h.links[l]; ok {
		_ = link.(endpointInternal).closed(err)
		delete(h.links, l)
		l.Free()
	}
}

func (h *handler) sessionClosed(ps proton.Session, err error) {
	if s, ok := h.sessions[ps]; ok {
		delete(h.sessions, ps)
		err = s.closed(err)
		for l, _ := range h.links {
			if l.Session() == ps {
				h.linkClosed(l, err)
			}
		}
		ps.Free()
	}
}

func (h *handler) shutdown(err error) {
	err = h.connection.closed(err)
	for _, sm := range h.sent {
		// Don't block but ensure outcome is sent eventually.
		if sm.ack != nil {
			o := Outcome{Unacknowledged, err, sm.v}
			select {
			case sm.ack <- o:
			default:
				go func(ack chan<- Outcome) { ack <- o }(sm.ack) // Deliver it eventually
			}
		}
	}
	h.sent = nil
	for _, l := range h.links {
		_ = l.(endpointInternal).closed(err)
	}
	h.links = nil
	for _, s := range h.sessions {
		_ = s.closed(err)
	}
	h.sessions = nil
}
