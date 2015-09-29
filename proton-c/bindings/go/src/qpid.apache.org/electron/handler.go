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

// FIXME aconway 2015-10-07: move to amqp or split into sub packages?
// proton.core
// proton.msg

package electron

import (
	"qpid.apache.org/proton"
	"qpid.apache.org/amqp"
)

// NOTE: methods in this file are called only in the proton goroutine unless otherwise indicated.

type handler struct {
	delegator    *proton.MessagingDelegator
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
	h.delegator = proton.NewMessagingDelegator(h)
	// Disable auto features of MessagingDelegator, we do these ourselves.
	h.delegator.Prefetch = 0
	h.delegator.AutoAccept = false
	h.delegator.AutoSettle = false
	h.delegator.AutoOpen = false
	return h
}

func (h *handler) HandleMessagingEvent(t proton.MessagingEvent, e proton.Event) {
	switch t {

	case proton.MMessage:
		if r, ok := h.links[e.Link()].(*receiver); ok {
			r.message(e.Delivery())
		} else {
			proton.CloseError(
				h.connection.eConnection,
				amqp.Errorf(amqp.InternalError, "no receiver for link %s", e.Link()))
		}

	case proton.MSettled:
		if sm := h.sentMessages[e.Delivery()]; sm != nil {
			sm.settled(nil)
		}

	case proton.MSendable:
		h.trySend(e.Link())

	case proton.MSessionOpening:
		if e.Session().State().LocalUninit() { // Remotely opened
			s := newSession(h.connection, e.Session())
			if err := h.connection.accept(s); err != nil {
				proton.CloseError(e.Session(), (err))
			} else {
				h.sessions[e.Session()] = s
				if s.capacity > 0 {
					e.Session().SetIncomingCapacity(s.capacity)
				}
				e.Session().Open()
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
		if l.State().LocalUninit() { // Remotely opened
			ss := h.sessions[l.Session()]
			if ss == nil {
				proton.CloseError(
					l, amqp.Errorf(amqp.InternalError, ("no session for link")))
				break
			}
			var link Link
			if l.IsReceiver() {
				r := &receiver{link: incomingLink(ss, l)}
				link = r
				r.inAccept = true
				defer func() { r.inAccept = false }()
			} else {
				link = &sender{link: incomingLink(ss, l)}
			}
			if err := h.connection.accept(link); err != nil {
				proton.CloseError(l, err)
				break
			}
			link.open()
		}

	case proton.MLinkOpened:
		l := e.Link()
		if l.IsSender() {
			h.trySend(l)
		}

	case proton.MLinkClosing:
		e.Link().Close()

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), proton.EndpointError(e.Link()))

	case proton.MDisconnected:
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

func (h *handler) trySend(l proton.Link) {
	if l.Credit() <= 0 {
		return
	}
	if s, ok := h.links[l].(*sender); ok {
		for ch := s.popBlocked(); l.Credit() > 0 && ch != nil; ch = s.popBlocked() {
			if snd, ok := <-ch; ok {
				s.doSend(snd)
			}
		}
	} else {
		h.connection.closed(
			amqp.Errorf(amqp.InternalError, "cannot find sender for link %s", l))
	}
}
