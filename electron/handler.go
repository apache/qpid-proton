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
	links        map[proton.Link]Link
	sentMessages map[proton.Delivery]*sentMessage
	sessions     map[proton.Session]*session
}

func newHandler(c *connection) *handler {
	h := &handler{
		connection:   c,
		links:        make(map[proton.Link]Link),
		sentMessages: make(map[proton.Delivery]*sentMessage),
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

func (h *handler) internalError(fmt string, arg ...interface{}) {
	proton.CloseError(h.connection.eConnection, amqp.Errorf(amqp.InternalError, fmt, arg...))
}

func (h *handler) HandleMessagingEvent(t proton.MessagingEvent, e proton.Event) {
	switch t {

	case proton.MMessage:
		if r, ok := h.links[e.Link()].(*receiver); ok {
			r.message(e.Delivery())
		} else {
			h.internalError("no receiver for link %s", e.Link())
		}

	case proton.MSettled:
		if sm := h.sentMessages[e.Delivery()]; sm != nil {
			sm.settled(nil)
		}

	case proton.MSendable:
		if s, ok := h.links[e.Link()].(*sender); ok {
			s.sendable()
		} else {
			h.internalError("no receiver for link %s", e.Link())
		}

	case proton.MSessionOpening:
		if e.Session().State().LocalUninit() { // Remotely opened
			incoming := &IncomingSession{h: h, pSession: e.Session()}
			h.connection.accept(incoming)
			if err := incoming.error(); err != nil {
				proton.CloseError(e.Session(), err)
			}
		}

	case proton.MSessionClosed:
		err := proton.EndpointError(e.Session())
		for l, _ := range h.links {
			if l.Session() == e.Session() {
				h.linkClosed(l, err)
			}
		}
		delete(h.sessions, e.Session())

	case proton.MLinkOpening:
		l := e.Link()
		if l.State().LocalActive() { // Already opened locally.
			break
		}
		ss := h.sessions[l.Session()]
		if ss == nil {
			h.internalError("no session for link %s", e.Link())
			break
		}
		var incoming Incoming
		if l.IsReceiver() {
			incoming = &IncomingReceiver{makeIncomingLink(ss, l)}
		} else {
			incoming = &IncomingSender{makeIncomingLink(ss, l)}
		}
		h.connection.accept(incoming)
		if err := incoming.error(); err != nil {
			proton.CloseError(l, err)
			break
		}

	case proton.MLinkClosing:
		e.Link().Close()

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), proton.EndpointError(e.Link()))

	case proton.MConnectionClosing:
		h.connection.err.Set(e.Connection().RemoteCondition().Error())

	case proton.MConnectionClosed:
		h.connection.err.Set(Closed) // If no error already set, this is an orderly close.

	case proton.MDisconnected:
		h.connection.err.Set(e.Transport().Condition().Error())
		// If err not set at this point (e.g. to Closed) then this is unexpected.
		h.connection.err.Set(amqp.Errorf(amqp.IllegalState, "unexpected disconnect on %s", h.connection))

		err := h.connection.Error()
		for l, _ := range h.links {
			h.linkClosed(l, err)
		}
		for _, s := range h.sessions {
			s.closed(err)
		}
		for _, sm := range h.sentMessages {
			sm.settled(err)
		}
	}
}

func (h *handler) linkClosed(l proton.Link, err error) {
	if link := h.links[l]; link != nil {
		link.closed(err)
		delete(h.links, l)
	}
}

func (h *handler) addLink(rl proton.Link, ll Link) {
	h.links[rl] = ll
}
