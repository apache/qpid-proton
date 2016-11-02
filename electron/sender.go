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
	"fmt"
	"qpid.apache.org/amqp"
	"qpid.apache.org/proton"
	"time"
)

// Sender is a Link that sends messages.
//
// The result of sending a message is provided by an Outcome value.
//
// A sender can buffer messages up to the credit limit provided by the remote receiver.
// All the Send* methods will block if the buffer is full until there is space.
// Send*Timeout methods will give up after the timeout and set Timeout as Outcome.Error.
//
type Sender interface {
	Endpoint
	LinkSettings

	// SendSync sends a message and blocks until the message is acknowledged by the remote receiver.
	// Returns an Outcome, which may contain an error if the message could not be sent.
	SendSync(m amqp.Message) Outcome

	// SendWaitable puts a message in the send buffer and returns a channel that
	// you can use to wait for the Outcome of just that message. The channel is
	// buffered so you can receive from it whenever you want without blocking.
	//
	// Note: can block if there is no space to buffer the message.
	SendWaitable(m amqp.Message) <-chan Outcome

	// SendForget buffers a message for sending and returns, with no notification of the outcome.
	//
	// Note: can block if there is no space to buffer the message.
	SendForget(m amqp.Message)

	// SendAsync puts a message in the send buffer and returns immediately.  An
	// Outcome with Value = value will be sent to the ack channel when the remote
	// receiver has acknowledged the message or if there is an error.
	//
	// You can use the same ack channel for many calls to SendAsync(), possibly on
	// many Senders. The channel will receive the outcomes in the order they
	// become available. The channel should be buffered and/or served by dedicated
	// goroutines to avoid blocking the connection.
	//
	// If ack == nil no Outcome is sent.
	//
	// Note: can block if there is no space to buffer the message.
	SendAsync(m amqp.Message, ack chan<- Outcome, value interface{})

	SendAsyncTimeout(m amqp.Message, ack chan<- Outcome, value interface{}, timeout time.Duration)

	SendWaitableTimeout(m amqp.Message, timeout time.Duration) <-chan Outcome

	SendForgetTimeout(m amqp.Message, timeout time.Duration)

	SendSyncTimeout(m amqp.Message, timeout time.Duration) Outcome
}

// Outcome provides information about the outcome of sending a message.
type Outcome struct {
	// Status of the message: was it sent, how was it acknowledged.
	Status SentStatus
	// Error is a local error if Status is Unsent or Unacknowledged, a remote error otherwise.
	Error error
	// Value provided by the application in SendAsync()
	Value interface{}
}

func (o Outcome) send(ack chan<- Outcome) {
	if ack != nil {
		ack <- o
	}
}

// SentStatus indicates the status of a sent message.
type SentStatus int

const (
	// Message was never sent
	Unsent SentStatus = iota
	// Message was sent but never acknowledged. It may or may not have been received.
	Unacknowledged
	// Message was accepted by the receiver (or was sent pre-settled, accept is assumed)
	Accepted
	// Message was rejected as invalid by the receiver
	Rejected
	// Message was not processed by the receiver but may be valid for a different receiver
	Released
	// Receiver responded with an unrecognized status.
	Unknown
)

// String human readable name for SentStatus.
func (s SentStatus) String() string {
	switch s {
	case Unsent:
		return "unsent"
	case Unacknowledged:
		return "unacknowledged"
	case Accepted:
		return "accepted"
	case Rejected:
		return "rejected"
	case Released:
		return "released"
	case Unknown:
		return "unknown"
	default:
		return fmt.Sprintf("invalid(%d)", s)
	}
}

// Convert proton delivery state code to SentStatus value
func sentStatus(d uint64) SentStatus {
	switch d {
	case proton.Accepted:
		return Accepted
	case proton.Rejected:
		return Rejected
	case proton.Released, proton.Modified:
		return Released
	default:
		return Unknown
	}
}

// Sender implementation, held by handler.
type sender struct {
	link
	credit chan struct{} // Signal available credit.
}

func (s *sender) SendAsyncTimeout(m amqp.Message, ack chan<- Outcome, v interface{}, t time.Duration) {
	// wait for credit
	if _, err := timedReceive(s.credit, t); err != nil {
		if err == Closed && s.Error() != nil {
			err = s.Error()
		}
		Outcome{Unsent, err, v}.send(ack)
		return
	}
	// Send a message in handler goroutine
	err := s.engine().Inject(func() {
		if s.Error() != nil {
			Outcome{Unsent, s.Error(), v}.send(ack)
			return
		}

		delivery, err2 := s.pLink.Send(m)
		switch {
		case err2 != nil:
			Outcome{Unsent, err2, v}.send(ack)
		case ack == nil || s.SndSettle() == SndSettled: // Pre-settled
			if s.SndSettle() != SndUnsettled { // Not forced to send unsettled by link policy
				delivery.Settle()
			}
			Outcome{Accepted, nil, v}.send(ack) // Assume accepted
		default:
			s.handler().sentMessages[delivery] = sentMessage{ack, v} // Register with handler
		}
		if s.pLink.Credit() > 0 { // Signal there is still credit
			s.sendable()
		}
	})
	if err != nil {
		Outcome{Unsent, err, v}.send(ack)
	}
}

// Set credit flag if not already set. Non-blocking, any goroutine
func (s *sender) sendable() {
	select { // Non-blocking
	case s.credit <- struct{}{}:
	default:
	}
}

func (s *sender) SendWaitableTimeout(m amqp.Message, t time.Duration) <-chan Outcome {
	out := make(chan Outcome, 1)
	s.SendAsyncTimeout(m, out, nil, t)
	return out
}

func (s *sender) SendForgetTimeout(m amqp.Message, t time.Duration) {
	s.SendAsyncTimeout(m, nil, nil, t)
}

func (s *sender) SendSyncTimeout(m amqp.Message, t time.Duration) Outcome {
	deadline := time.Now().Add(t)
	ack := s.SendWaitableTimeout(m, t)
	t = deadline.Sub(time.Now()) // Adjust for time already spent.
	if t < 0 {
		t = 0
	}
	if out, err := timedReceive(ack, t); err == nil {
		return out.(Outcome)
	} else {
		if err == Closed && s.Error() != nil {
			err = s.Error()
		}
		return Outcome{Unacknowledged, err, nil}
	}
}

func (s *sender) SendAsync(m amqp.Message, ack chan<- Outcome, v interface{}) {
	s.SendAsyncTimeout(m, ack, v, Forever)
}

func (s *sender) SendWaitable(m amqp.Message) <-chan Outcome {
	return s.SendWaitableTimeout(m, Forever)
}

func (s *sender) SendForget(m amqp.Message) {
	s.SendForgetTimeout(m, Forever)
}

func (s *sender) SendSync(m amqp.Message) Outcome {
	return <-s.SendWaitable(m)
}

// handler goroutine
func (s *sender) closed(err error) error {
	close(s.credit)
	return s.link.closed(err)
}

func newSender(ls linkSettings) *sender {
	s := &sender{link: link{linkSettings: ls}, credit: make(chan struct{}, 1)}
	s.endpoint.init(s.link.pLink.String())
	s.handler().addLink(s.pLink, s)
	s.link.pLink.Open()
	return s
}

// sentMessage records a sent message on the handler.
type sentMessage struct {
	ack   chan<- Outcome
	value interface{}
}

// IncomingSender is sent on the Connection.Incoming() channel when there is
// an incoming request to open a sender link.
type IncomingSender struct {
	incoming
	linkSettings
}

func newIncomingSender(sn *session, pLink proton.Link) *IncomingSender {
	return &IncomingSender{
		incoming:     makeIncoming(pLink),
		linkSettings: makeIncomingLinkSettings(pLink, sn),
	}
}

// Accept accepts an incoming sender endpoint
func (in *IncomingSender) Accept() Endpoint {
	return in.accept(func() Endpoint { return newSender(in.linkSettings) })
}

// Call in injected functions to check if the sender is valid.
func (s *sender) valid() bool {
	s2, ok := s.handler().links[s.pLink].(*sender)
	return ok && s2 == s
}
