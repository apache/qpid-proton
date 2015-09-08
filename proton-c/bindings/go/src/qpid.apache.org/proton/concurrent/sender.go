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

// #include <proton/disposition.h>
import "C"

import (
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/amqp"
	"qpid.apache.org/proton/internal"
	"time"
)

type SenderSettings struct {
	LinkSettings
}

// Sender is a Link that sends messages.
type Sender interface {
	Link

	// Send a message asynchronously, return a SentMessage to identify it.
	//
	// Returns nil if the link is in Unreliable mode and no acknowledgement
	// will be received.
	//
	// See Credit() for note on buffering.
	//
	// Use SentMessage.Disposition() to wait for acknowledgement.
	Send(m amqp.Message) (sm SentMessage, err error)
}

type sender struct{ link }

func newSender(l link) Sender { return &sender{l} }

// Open the Sender, must be called before calling Send().
func (s *sender) Open() error {
	err := s.engine().InjectWait(func() error {
		err := s.open()
		if err == nil {
			s.handler().addLink(s.eLink, s)
		}
		return err
	})
	return s.setError(err)
}

// Disposition indicates the outcome of a settled message delivery.
type Disposition uint64

const (
	// No disposition available, not yet acknowledged or an error occurred
	NoDisposition Disposition = 0
	// Message was accepted by the receiver
	Accepted = proton.Accepted
	// Message was rejected as invalid by the receiver
	Rejected = proton.Rejected
	// Message was not processed by the receiver but may be processed by some other receiver.
	Released = proton.Released
)

// String human readable name for a Disposition.
func (d Disposition) String() string {
	switch d {
	case NoDisposition:
		return "no-disposition"
	case Accepted:
		return "accepted"
	case Rejected:
		return "rejected"
	case Released:
		return "released"
	default:
		return "unknown"
	}
}

func (s *sender) Send(m amqp.Message) (SentMessage, error) {
	internal.Assert(s.IsOpen(), "sender is not open: %s", s)
	if err := s.Error(); err != nil {
		return nil, err
	}
	var sm SentMessage
	err := s.engine().InjectWait(func() error {
		eDelivery, err := s.eLink.Send(m)
		if err == nil {
			if s.eLink.SndSettleMode() == proton.SndSettled {
				eDelivery.Settle()
			} else {
				sm = newSentMessage(s.session.connection, eDelivery)
				s.session.connection.handler.sentMessages[eDelivery] = sm.(*sentMessage)
			}
		}
		return err
	})
	return sm, err
}

func (s *sender) closed(err error) {
	s.closeError(err)
}

// SentMessage represents a previously sent message. It allows you to wait for acknowledgement.
type SentMessage interface {
	// Disposition blocks till the message is acknowledged and returns the
	// disposition state.  NoDisposition means the Connection or the SentMessage
	// was closed before the message was acknowledged.
	Disposition() (Disposition, error)

	// DispositionTimeout is like Disposition but gives up after timeout, see Timeout.
	DispositionTimeout(time.Duration) (Disposition, error)

	// Forget interrupts any call to Disposition on this SentMessage and tells the
	// peer we are no longer interested in the disposition of this message.
	Forget()

	// Error returns the error that closed the disposition, or nil if there was no error.
	// If the disposition closed because the connection closed, it will return Closed.
	Error() error
}

type sentMessage struct {
	connection  *connection
	eDelivery   proton.Delivery
	done        chan struct{}
	disposition Disposition
	err         error
}

func newSentMessage(c *connection, d proton.Delivery) *sentMessage {
	return &sentMessage{c, d, make(chan struct{}), NoDisposition, nil}
}

func (sm *sentMessage) Disposition() (Disposition, error) {
	<-sm.done
	return sm.disposition, sm.err
}

func (sm *sentMessage) DispositionTimeout(timeout time.Duration) (Disposition, error) {
	if _, _, timedout := timedReceive(sm.done, timeout); timedout {
		return sm.disposition, Timeout
	} else {
		return sm.disposition, sm.err
	}
}

func (sm *sentMessage) Forget() {
	sm.connection.engine.Inject(func() {
		sm.eDelivery.Settle()
		delete(sm.connection.handler.sentMessages, sm.eDelivery)
	})
	sm.finish()
}

func (sm *sentMessage) settled(err error) {
	if sm.eDelivery.Settled() {
		sm.disposition = Disposition(sm.eDelivery.Remote().Type())
	}
	sm.err = err
	sm.finish()
}

func (sm *sentMessage) finish() {
	select {
	case <-sm.done: // No-op if already closed
	default:
		close(sm.done)
	}
}

func (sm *sentMessage) Error() error { return sm.err }
