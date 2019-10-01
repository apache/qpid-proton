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
	"github.com/apache/qpid-proton/go/pkg/proton"
)

// Session is an AMQP session, it contains Senders and Receivers.
type Session interface {
	Endpoint

	// Sender opens a new sender.
	Sender(...LinkOption) (Sender, error)

	// Receiver opens a new Receiver.
	Receiver(...LinkOption) (Receiver, error)
}

type session struct {
	endpoint
	pSession                         proton.Session
	connection                       *connection
	incomingCapacity, outgoingWindow uint
}

// SessionOption can be passed when creating a Session
type SessionOption func(*session)

// IncomingCapacity returns a Session Option that sets the size (in bytes) of
// the session's incoming data buffer.
func IncomingCapacity(bytes uint) SessionOption {
	return func(s *session) { s.incomingCapacity = bytes }
}

// OutgoingWindow returns a Session Option that sets the outgoing window size (in frames).
func OutgoingWindow(frames uint) SessionOption {
	return func(s *session) { s.outgoingWindow = frames }
}

// in proton goroutine
func newSession(c *connection, es proton.Session, setting ...SessionOption) *session {
	s := &session{
		connection: c,
		pSession:   es,
	}
	s.endpoint.init(es.String())
	for _, set := range setting {
		set(s)
	}
	c.handler.sessions[s.pSession] = s
	s.pSession.SetIncomingCapacity(s.incomingCapacity)
	s.pSession.SetOutgoingWindow(s.outgoingWindow)
	s.pSession.Open()
	return s
}

func (s *session) Connection() Connection     { return s.connection }
func (s *session) pEndpoint() proton.Endpoint { return s.pSession }
func (s *session) engine() *proton.Engine     { return s.connection.engine }

func (s *session) Close(err error) {
	_ = s.engine().Inject(func() {
		if s.Error() == nil {
			localClose(s.pSession, err)
		}
	})
}

func (s *session) Sender(setting ...LinkOption) (snd Sender, err error) {
	err = s.engine().InjectWait(func() error {
		if s.Error() != nil {
			return s.Error()
		}
		l, err2 := makeLocalLink(s, true, setting...)
		if err2 == nil {
			snd = newSender(l)
		}
		return err2
	})
	return
}

func (s *session) Receiver(setting ...LinkOption) (rcv Receiver, err error) {
	err = s.engine().InjectWait(func() error {
		if s.Error() != nil {
			return s.Error()
		}
		l, err2 := makeLocalLink(s, false, setting...)
		if err2 == nil {
			rcv = newReceiver(l)
		}
		return err2
	})
	return
}

// IncomingSender is sent on the Connection.Incoming() channel when there is an
// incoming request to open a session.
type IncomingSession struct {
	incoming
	h                                *handler
	pSession                         proton.Session
	incomingCapacity, outgoingWindow uint
}

func newIncomingSession(h *handler, ps proton.Session) *IncomingSession {
	return &IncomingSession{incoming: makeIncoming(ps), h: h, pSession: ps}
}

// SetIncomingCapacity sets the session buffer capacity of an incoming session in bytes.
func (in *IncomingSession) SetIncomingCapacity(bytes uint) { in.incomingCapacity = bytes }

// SetOutgoingWindow sets the session outgoing window of an incoming session in frames.
func (in *IncomingSession) SetOutgoingWindow(frames uint) { in.outgoingWindow = frames }

// Accept an incoming session endpoint.
func (in *IncomingSession) Accept() Endpoint {
	return in.accept(func() Endpoint {
		return newSession(in.h.connection, in.pSession, IncomingCapacity(in.incomingCapacity), OutgoingWindow(in.outgoingWindow))
	})
}
