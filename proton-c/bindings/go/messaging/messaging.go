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

package messaging

// #include <proton/disposition.h>
import "C"

import (
	"net"
	"qpid.apache.org/proton/go/amqp"
	"qpid.apache.org/proton/go/event"
)

// Connection is a connection to a remote AMQP endpoint.
//
// You can set exported fields to configure the connection before calling
// Connection.Open()
//
type Connection struct {
	// Server = true means a the connection will do automatic protocol detection.
	Server bool

	// FIXME aconway 2015-04-17: Other parameters to set up SSL, SASL etc.

	handler *handler
	pump    *event.Pump
	session Session
}

// Make an AMQP connection over a net.Conn connection.
//
// Use Connection.Close() to close the Connection, this will also close conn.
// Using conn.Close() directly will cause an abrupt disconnect rather than an
// orderly AMQP close.
//
func (c *Connection) Open(conn net.Conn) (err error) {
	c.handler = newHandler(c)
	c.pump, err = event.NewPump(conn,
		event.NewMessagingDelegator(c.handler),
	)
	if err != nil {
		return err
	}
	if c.Server {
		c.pump.Server()
	}
	go c.pump.Run()
	return nil
}

// Connect opens a default client connection. It is a shortcut for
//    c := &Connection
//    c.Open()
//
func Connect(conn net.Conn) (*Connection, error) {
	c := &Connection{}
	err := c.Open(conn)
	return c, err
}

// Close the connection.
//
// Connections must be closed to clean up resources and stop associated goroutines.
func (c *Connection) Close() error { return c.pump.Close() }

// DefaultSession returns a default session for the connection.
//
// It is created on the first call to DefaultSession() and returned from all subsequent calls.
// Use Session() for more control over creating sessions.
//
func (c *Connection) DefaultSession() (s Session, err error) {
	if c.session.e.IsNil() {
		c.session, err = c.Session()
	}
	return c.session, err
}

type sessionErr struct {
	s   event.Session
	err error
}

// Session creates a new session.
func (c *Connection) Session() (Session, error) {
	connection := c.pump.Connection()
	result := make(chan sessionErr)
	c.pump.Inject <- func() {
		s, err := connection.Session()
		if err == nil {
			s.Open()
		}
		result <- sessionErr{s, err}
	}
	se := <-result
	return Session{se.s, c.pump}, se.err
}

// FIXME aconway 2015-04-27: set sender name, options etc.

// Sender creates a Sender that will send messages to the address addr.
func (c *Connection) Sender(addr string) (s Sender, err error) {
	session, err := c.DefaultSession()
	if err != nil {
		return Sender{}, err
	}
	result := make(chan Sender)
	c.pump.Inject <- func() {
		link := session.e.Sender(linkNames.Next())
		if link.IsNil() {
			err = session.e.Error()
		} else {
			link.Target().SetAddress(addr)
			// FIXME aconway 2015-04-27: link options?
			link.Open()
		}
		result <- Sender{Link{c, link}}
	}
	return <-result, err
}

// Receiver returns a receiver that will receive messages sent to address addr.
func (c *Connection) Receiver(addr string) (r Receiver, err error) {
	// FIXME aconway 2015-04-29: move code to session, in link.go?
	session, err := c.DefaultSession()
	if err != nil {
		return Receiver{}, err
	}
	result := make(chan Receiver)
	c.pump.Inject <- func() {
		link := session.e.Receiver(linkNames.Next())
		if link.IsNil() {
			err = session.e.Error()
		} else {
			link.Source().SetAddress(addr)
			// FIXME aconway 2015-04-27: link options?
			link.Open()
		}
		// FIXME aconway 2015-04-29: hack to avoid blocking, need proper buffering linked to flow control
		rchan := make(chan amqp.Message, 1000)
		c.handler.receivers[link] = rchan
		result <- Receiver{Link{c, link}, rchan}
	}
	return <-result, err
}

// FIXME aconway 2015-04-29: counter per session.
var linkNames amqp.UidCounter

// Session is an AMQP session, it contains Senders and Receivers.
// Every Connection has a DefaultSession, you can create additional sessions
// with Connection.Session()
type Session struct {
	e    event.Session
	pump *event.Pump
}

// FIXME aconway 2015-05-05: REWORK Sender/receiver/session.

// Disposition indicates the outcome of a settled message delivery.
type Disposition uint64

const (
	// Message was accepted by the receiver
	Accepted Disposition = C.PN_ACCEPTED
	// Message was rejected as invalid by the receiver
	Rejected = C.PN_REJECTED
	// Message was not processed by the receiver but may be processed by some other receiver.
	Released = C.PN_RELEASED
)

// String human readable name for a Disposition.
func (d Disposition) String() string {
	switch d {
	case Accepted:
		return "Accepted"
	case Rejected:
		return "Rejected"
	case Released:
		return "Released"
	default:
		return "Unknown"
	}
}

// FIXME aconway 2015-04-29: How to signal errors via ack channels.

// An Acknowledgement is a channel which will receive the Disposition of the message
// when it is acknowledged. The channel is closed after the disposition is sent.
type Acknowledgement <-chan Disposition

// Link has common data and methods for Sender and Receiver links.
type Link struct {
	connection *Connection
	elink      event.Link
}

// Sender sends messages.
type Sender struct {
	Link
}

// FIXME aconway 2015-04-28: allow user to specify delivery tag.
// FIXME aconway 2015-04-28: should we provide a sending channel rather than a send function?

// Send sends a message. If d is not nil, the disposition is retured on d.
// If d is nil the message is sent pre-settled and no disposition is returned.
func (s *Sender) Send(m amqp.Message) (ack Acknowledgement, err error) {
	ackChan := make(chan Disposition, 1)
	ack = ackChan
	s.connection.pump.Inject <- func() {
		// FIXME aconway 2015-04-28: flow control & credit, buffer or fail?
		delivery, err := s.elink.Send(m)
		if err == nil { // FIXME aconway 2015-04-28: error handling
			s.connection.handler.acks[delivery] = ackChan
		}
	}
	return ack, nil
}

// Close the sender.
func (s *Sender) Close() error { return nil } // FIXME aconway 2015-04-27: close/free

// Receiver receives messages via the channel Receive.
type Receiver struct {
	Link
	// Channel of messag
	Receive <-chan amqp.Message
}

// FIXME aconway 2015-04-29: settlement - ReceivedMessage with Settle() method?

// Close the Receiver.
func (r *Receiver) Close() error { return nil } // FIXME aconway 2015-04-29: close/free
