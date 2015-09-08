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
	"qpid.apache.org/proton/internal"
)

// Session is an AMQP session, it contains Senders and Receivers.
//
type Session interface {
	Endpoint
	Connection() Connection

	// Sender opens a new sender. v can be a string, which is used as the Target
	// address, or a SenderSettings struct containing more details settings.
	Sender(v interface{}) (Sender, error)

	// Receiver opens a new Receiver. v can be a string, which is used as the
	// Source address, or a ReceiverSettings struct containing more details
	// settings.
	Receiver(v interface{}) (Receiver, error)
}

type session struct {
	endpoint
	eSession   proton.Session
	connection *connection
}

// in proton goroutine
func newSession(c *connection, es proton.Session) *session {
	return &session{
		connection: c,
		eSession:   es,
		endpoint:   endpoint{str: es.String()},
	}
}

func (s *session) Connection() Connection     { return s.connection }
func (s *session) eEndpoint() proton.Endpoint { return s.eSession }
func (s *session) engine() *proton.Engine     { return s.connection.engine }
func (s *session) Open() error                { s.engine().Inject(s.eSession.Open); return nil }
func (s *session) Close(err error) {
	s.engine().Inject(func() { localClose(s.eSession, err) })
}

// NewSender create a link sending to target.
// You must call snd.Open() before calling snd.Send().
func (s *session) Sender(v interface{}) (snd Sender, err error) {
	var settings LinkSettings
	switch v := v.(type) {
	case string:
		settings.Target = v
	case SenderSettings:
		settings = v.LinkSettings
	default:
		internal.Assert(false, "NewSender() want string or SenderSettings, got %T", v)
	}
	err = s.engine().InjectWait(func() error {
		l, err := makeLocalLink(s, true, settings)
		snd = newSender(l)
		return err
	})
	if err == nil {
		err = snd.Open()
	}
	return
}

// Receiver opens a receiving link.
func (s *session) Receiver(v interface{}) (rcv Receiver, err error) {
	var settings ReceiverSettings
	switch v := v.(type) {
	case string:
		settings.Source = v
	case ReceiverSettings:
		settings = v
	default:
		internal.Assert(false, "NewReceiver() want string or ReceiverSettings, got %T", v)
	}
	err = s.engine().InjectWait(func() error {
		l, err := makeLocalLink(s, false, settings.LinkSettings)
		rcv = newReceiver(l)
		return err
	})
	rcv.SetCapacity(settings.Capacity, settings.Prefetch)
	if err == nil {
		err = rcv.Open()
	}
	return
}

// Called from handler on closed.
func (s *session) closed(err error) {
	s.closeError(err)
}
