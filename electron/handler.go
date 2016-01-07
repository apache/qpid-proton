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
	"qpid.apache.org/amqp"
	"qpid.apache.org/proton"
)

// NOTE: methods in this file are called only in the proton goroutine unless otherwise indicated.

type handler struct {
	delegator    *proton.MessagingAdapter
	connection   *connection
	links        map[proton.Link]Endpoint
	sentMessages map[proton.Delivery]sentMessage
	sessions     map[proton.Session]*session
}

func newHandler(c *connection) *handler {
	h := &handler{
		connection:   c,
		links:        make(map[proton.Link]Endpoint),
		sentMessages: make(map[proton.Delivery]sentMessage),
		sessions:     make(map[proton.Session]*session),
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
		if sm, ok := h.sentMessages[e.Delivery()]; ok {
			d := e.Delivery().Remote()
			sm.ack <- Outcome{sentStatus(d.Type()), d.Condition().Error(), sm.value}
			delete(h.sentMessages, e.Delivery())
		}

	case proton.MSendable:
		if s, ok := h.links[e.Link()].(*sender); ok {
			s.sendable()
		} else {
			h.linkError(e.Link(), "no sender")
		}

	case proton.MSessionOpening:
		if e.Session().State().LocalUninit() { // Remotely opened
			h.incoming(newIncomingSession(h, e.Session()))
		}

	case proton.MSessionClosed:
		h.sessionClosed(e.Session(), proton.EndpointError(e.Session()))

	case proton.MLinkOpening:
		l := e.Link()
		if l.State().LocalActive() { // Already opened locally.
			break
		}
		ss := h.sessions[l.Session()]
		if ss == nil {
			h.linkError(e.Link(), "no session")
			break
		}
		if l.IsReceiver() {
			h.incoming(&IncomingReceiver{makeIncomingLink(ss, l)})
		} else {
			h.incoming(&IncomingSender{makeIncomingLink(ss, l)})
		}

	case proton.MLinkClosing:
		e.Link().Close()

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), proton.EndpointError(e.Link()))

	case proton.MConnectionClosing:
		h.connection.err.Set(e.Connection().RemoteCondition().Error())

	case proton.MConnectionClosed:
		h.connectionClosed(proton.EndpointError(e.Connection()))

	case proton.MDisconnected:
		h.connection.err.Set(e.Transport().Condition().Error())
		// If err not set at this point (e.g. to Closed) then this is unexpected.
		h.connection.err.Set(amqp.Errorf(amqp.IllegalState, "unexpected disconnect on %s", h.connection))

		err := h.connection.Error()

		for l, _ := range h.links {
			h.linkClosed(l, err)
		}
		h.links = nil
		for _, s := range h.sessions {
			s.closed(err)
		}
		h.sessions = nil
		for _, sm := range h.sentMessages {
			sm.ack <- Outcome{Unacknowledged, err, sm.value}
		}
		h.sentMessages = nil
	}
}

func (h *handler) incoming(in Incoming) {
	var err error
	if h.connection.incoming != nil {
		h.connection.incoming <- in
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
		link.closed(err)
		delete(h.links, l)
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
	}
}

func (h *handler) connectionClosed(err error) {
	err = h.connection.closed(err)
	// Close links first to avoid repeated scans of the link list by sessions.
	for l, _ := range h.links {
		h.linkClosed(l, err)
	}
	for s, _ := range h.sessions {
		h.sessionClosed(s, err)
	}
}
