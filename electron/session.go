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
	"qpid.apache.org/proton"
)

// Session is an AMQP session, it contains Senders and Receivers.
type Session interface {
	Endpoint

	// Sender opens a new sender. v can be a string, which is used as the Target
	// address, or a SenderSettings struct containing more details settings.
	Sender(...LinkSetting) (Sender, error)

	// Receiver opens a new Receiver. v can be a string, which is used as the
	// Source address, or a ReceiverSettings struct containing more details
	// settings.
	Receiver(...LinkSetting) (Receiver, error)
}

type session struct {
	endpoint
	eSession   proton.Session
	connection *connection
	capacity   uint
}

// SessionSetting can be passed when creating a sender or receiver.
// See functions that return SessionSetting for details
type SessionSetting func(*session)

// IncomingCapacity sets the size (in bytes) of the sessions incoming data buffer..
func IncomingCapacity(cap uint) SessionSetting { return func(s *session) { s.capacity = cap } }

// in proton goroutine
func newSession(c *connection, es proton.Session, setting ...SessionSetting) *session {
	s := &session{
		connection: c,
		eSession:   es,
		endpoint:   endpoint{str: es.String()},
	}
	for _, set := range setting {
		set(s)
	}
	c.handler.sessions[s.eSession] = s
	s.eSession.SetIncomingCapacity(s.capacity)
	s.eSession.Open()
	return s
}

func (s *session) Connection() Connection     { return s.connection }
func (s *session) eEndpoint() proton.Endpoint { return s.eSession }
func (s *session) engine() *proton.Engine     { return s.connection.engine }
func (s *session) Close(err error) {
	s.engine().Inject(func() { localClose(s.eSession, err) })
}

func (s *session) SetCapacity(bytes uint) { s.capacity = bytes }

func (s *session) Sender(setting ...LinkSetting) (snd Sender, err error) {
	err = s.engine().InjectWait(func() error {
		l, err := localLink(s, true, setting...)
		if err == nil {
			snd = newSender(l)
		}
		return err
	})
	return
}

func (s *session) Receiver(setting ...LinkSetting) (rcv Receiver, err error) {
	err = s.engine().InjectWait(func() error {
		l, err := localLink(s, false, setting...)
		if err == nil {
			rcv = newReceiver(l)
		}
		return err
	})
	return
}

// Called from handler on closed.
func (s *session) closed(err error) {
	s.err.Set(err)
	s.err.Set(Closed)
}

// IncomingSession is passed to the accept() function given to Connection.Listen()
// when there is an incoming session request.
type IncomingSession struct {
	incoming
	h        *handler
	pSession proton.Session
	capacity uint
}

// AcceptCapacity sets the session buffer capacity of an incoming session in bytes.
func (i *IncomingSession) AcceptSession(bytes uint) Session {
	i.capacity = bytes
	return i.Accept().(Session)
}

func (i *IncomingSession) Accept() Endpoint {
	i.accepted = true
	return newSession(i.h.connection, i.pSession, IncomingCapacity(i.capacity))
}
