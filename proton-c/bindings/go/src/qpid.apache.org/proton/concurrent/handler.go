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

package concurrent

import (
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/amqp"
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
			r.handleDelivery(e.Delivery())
		} else {
			h.connection.closed(
				amqp.Errorf(amqp.InternalError, "cannot find receiver for link %s", e.Link()))
		}

	case proton.MSettled:
		if sm := h.sentMessages[e.Delivery()]; sm != nil {
			sm.settled(nil)
		}

	case proton.MSessionOpening:
		if e.Session().State().LocalUninit() { // Remotely opened
			s := newSession(h.connection, e.Session())
			h.sessions[e.Session()] = s
			if h.connection.incoming != nil {
				h.connection.incoming.In <- s
			} else {
				proton.CloseError(e.Session(), amqp.Errorf(amqp.NotAllowed, "remote sessions not allowed"))
			}
		}

	case proton.MSessionClosing:
		e.Session().Close()

	case proton.MSessionClosed:
		err := e.Session().RemoteCondition().Error()
		for l, _ := range h.links {
			if l.Session() == e.Session() {
				h.linkClosed(l, err)
			}
		}
		delete(h.sessions, e.Session())

	case proton.MLinkOpening:
		l := e.Link()
		if l.State().LocalUninit() { // Remotely opened
			if h.connection.incoming == nil {
				proton.CloseError(l, amqp.Errorf(amqp.NotAllowed, ("no remote links")))
				break
			}
			s := h.sessions[l.Session()]
			if s == nil {
				proton.CloseError(
					l, amqp.Errorf(amqp.InternalError, ("cannot find session for link")))
				break
			}
			h.connection.handleIncoming(s, l)
		}

	case proton.MLinkClosing:
		e.Link().Close()

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), e.Link().RemoteCondition().Error())

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
