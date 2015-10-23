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

// #include <proton/disposition.h>
import "C"

import (
	"qpid.apache.org/amqp"
	"qpid.apache.org/proton"
	"reflect"
	"time"
)

// Sender is a Link that sends messages.
type Sender interface {
	Link

	// Send a message without waiting for acknowledgement. Returns a SentMessage.
	// use SentMessage.Disposition() to wait for acknowledgement and get the
	// disposition code.
	//
	// If the send buffer is full, send blocks until there is space in the buffer.
	Send(m amqp.Message) (sm SentMessage, err error)

	// SendTimeout is like send but only waits up to timeout for buffer space.
	//
	// Returns Timeout error if the timeout expires and the message has not been sent.
	SendTimeout(m amqp.Message, timeout time.Duration) (sm SentMessage, err error)

	// Send a message and forget it, there will be no acknowledgement.
	// If the send buffer is full, send blocks until there is space in the buffer.
	SendForget(m amqp.Message) error

	// SendForgetTimeout is like send but only waits up to timeout for buffer space.
	// Returns Timeout error if the timeout expires and the message has not been sent.
	SendForgetTimeout(m amqp.Message, timeout time.Duration) error

	// Credit indicates how many messages the receiving end of the link can accept.
	//
	// On a Sender credit can be negative, meaning that messages in excess of the
	// receiver's credit limit have been buffered locally till credit is available.
	Credit() (int, error)
}

type sendMessage struct {
	m  amqp.Message
	sm SentMessage
}

type sender struct {
	link
	credit chan struct{} // Signal available credit.
}

// Disposition indicates the outcome of a settled message delivery.
type Disposition uint64

const (
	// No disposition available: pre-settled, not yet acknowledged or an error occurred
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
	return s.SendTimeout(m, Forever)
}

func (s *sender) SendTimeout(m amqp.Message, timeout time.Duration) (SentMessage, error) {
	var sm SentMessage
	if s.sndSettle == SndSettled {
		sm = nil
	} else {
		sm = newSentMessage(s.session.connection)
	}
	return s.sendInternal(sendMessage{m, sm}, timeout)
}

func (s *sender) SendForget(m amqp.Message) error {
	return s.SendForgetTimeout(m, Forever)
}

func (s *sender) SendForgetTimeout(m amqp.Message, timeout time.Duration) error {
	snd := sendMessage{m, nil}
	_, err := s.sendInternal(snd, timeout)
	return err
}

func (s *sender) sendInternal(snd sendMessage, timeout time.Duration) (SentMessage, error) {
	if _, err := timedReceive(s.credit, timeout); err != nil { // Wait for credit
		if err == Closed {
			err = s.Error()
			assert(err != nil)
		}
		return nil, err
	}
	if err := s.engine().Inject(func() { s.doSend(snd) }); err != nil {
		return nil, err
	}
	return snd.sm, nil
}

// Send a message. Handler goroutine
func (s *sender) doSend(snd sendMessage) {
	delivery, err := s.eLink.Send(snd.m)
	switch sm := snd.sm.(type) {
	case nil:
		delivery.Settle()
	case *sentMessage:
		sm.delivery = delivery
		if err != nil {
			sm.settled(err)
		} else {
			s.handler().sentMessages[delivery] = sm
		}
	default:
		assert(false, "bad SentMessage type %T", snd.sm)
	}
	if s.eLink.Credit() > 0 {
		s.sendable() // Signal credit.
	}
}

// Signal the sender has credit. Any goroutine.
func (s *sender) sendable() {
	select { // Non-blocking
	case s.credit <- struct{}{}: // Set the flag if not already set.
	default:
	}
}

func (s *sender) closed(err error) {
	s.link.closed(err)
	close(s.credit)
}

func newSender(l link) *sender {
	s := &sender{link: l, credit: make(chan struct{}, 1)}
	s.handler().addLink(s.eLink, s)
	s.link.open()
	return s
}

// SentMessage represents a previously sent message. It allows you to wait for acknowledgement.
type SentMessage interface {

	// Disposition blocks till the message is acknowledged and returns the
	// disposition state.
	//
	// NoDisposition with Error() != nil means the Connection was closed before
	// the message was acknowledged.
	//
	// NoDisposition with Error() == nil means the message was pre-settled or
	// Forget() was called.
	Disposition() (Disposition, error)

	// DispositionTimeout is like Disposition but gives up after timeout, see Timeout.
	DispositionTimeout(time.Duration) (Disposition, error)

	// Forget interrupts any call to Disposition on this SentMessage and tells the
	// peer we are no longer interested in the disposition of this message.
	Forget()

	// Error returns the error that closed the disposition, or nil if there was no error.
	// If the disposition closed because the connection closed, it will return Closed.
	Error() error

	// Value is an optional value you wish to associate with the SentMessage. It
	// can be the message itself or some form of identifier.
	Value() interface{}
	SetValue(interface{})
}

// SentMessageSet is a concurrent-safe set of sent messages that can be checked
// to get the next completed sent message
type SentMessageSet struct {
	cases []reflect.SelectCase
	sm    []SentMessage
	done  chan SentMessage
}

func (s *SentMessageSet) Add(sm SentMessage) {
	s.sm = append(s.sm, sm)
	s.cases = append(s.cases, reflect.SelectCase{Dir: reflect.SelectRecv, Chan: reflect.ValueOf(sm.(*sentMessage).done)})
}

// Wait waits up to timeout and returns the next SentMessage that has a valid dispositionb
// or an error.
func (s *SentMessageSet) Wait(sm SentMessage, timeout time.Duration) (SentMessage, error) {
	s.cases = s.cases[:len(s.sm)] // Remove previous timeout cases
	if timeout == 0 {             // Non-blocking
		s.cases = append(s.cases, reflect.SelectCase{Dir: reflect.SelectDefault})
	} else {
		s.cases = append(s.cases,
			reflect.SelectCase{Dir: reflect.SelectRecv, Chan: reflect.ValueOf(After(timeout))})
	}
	chosen, _, _ := reflect.Select(s.cases)
	if chosen > len(s.sm) {
		return nil, Timeout
	} else {
		sm := s.sm[chosen]
		s.sm = append(s.sm[:chosen], s.sm[chosen+1:]...)
		return sm, nil
	}
}

// SentMessage implementation
type sentMessage struct {
	connection  *connection
	done        chan struct{}
	delivery    proton.Delivery
	disposition Disposition
	err         error
	value       interface{}
}

func newSentMessage(c *connection) *sentMessage {
	return &sentMessage{connection: c, done: make(chan struct{})}
}

func (sm *sentMessage) SetValue(v interface{}) { sm.value = v }
func (sm *sentMessage) Value() interface{}     { return sm.value }
func (sm *sentMessage) Disposition() (Disposition, error) {
	<-sm.done
	return sm.disposition, sm.err
}

func (sm *sentMessage) DispositionTimeout(timeout time.Duration) (Disposition, error) {
	if _, err := timedReceive(sm.done, timeout); err == Timeout {
		return sm.disposition, Timeout
	} else {
		return sm.disposition, sm.err
	}
}

func (sm *sentMessage) Forget() {
	sm.connection.engine.Inject(func() {
		sm.delivery.Settle()
		delete(sm.connection.handler.sentMessages, sm.delivery)
	})
	sm.finish()
}

func (sm *sentMessage) settled(err error) {
	if sm.delivery.Settled() {
		sm.disposition = Disposition(sm.delivery.Remote().Type())
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

// IncomingSender is passed to the accept() function given to Connection.Listen()
// when there is an incoming request for a sender link.
type IncomingSender struct {
	incomingLink
}

// Link provides information about the incoming link.
func (i *IncomingSender) Link() Link { return i }

func (i *IncomingSender) AcceptSender() Sender { return i.Accept().(Sender) }

func (i *IncomingSender) Accept() Endpoint {
	i.accepted = true
	return newSender(i.link)
}
