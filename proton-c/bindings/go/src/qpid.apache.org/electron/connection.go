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
	"net"
	"qpid.apache.org/amqp"
	"qpid.apache.org/internal"
	"qpid.apache.org/proton"
	"sync"
	"time"
)

// Connection is an AMQP connection, created by a Container.
type Connection interface {
	Endpoint

	// Sender opens a new sender on the DefaultSession.
	//
	// v can be a string, which is used as the Target address, or a SenderSettings
	// struct containing more details settings.
	Sender(...LinkSetting) (Sender, error)

	// Receiver opens a new Receiver on the DefaultSession().
	//
	// v can be a string, which is used as the
	// Source address, or a ReceiverSettings struct containing more details
	// settings.
	Receiver(...LinkSetting) (Receiver, error)

	// DefaultSession() returns a default session for the connection. It is opened
	// on the first call to DefaultSession and returned on subsequent calls.
	DefaultSession() (Session, error)

	// Session opens a new session.
	Session(...SessionSetting) (Session, error)

	// Container for the connection.
	Container() Container

	// Disconnect the connection abruptly with an error.
	Disconnect(error)

	// Wait waits for the connection to be disconnected.
	Wait() error

	// WaitTimeout is like Wait but returns Timeout if the timeout expires.
	WaitTimeout(time.Duration) error
}

// ConnectionSetting can be passed when creating a connection.
// See functions that return ConnectionSetting for details
type ConnectionSetting func(*connection)

// Server setting puts the connection in server mode.
//
// A server connection will do protocol negotiation to accept a incoming AMQP
// connection. Normally you would call this for a connection created by
// net.Listener.Accept()
//
func Server() ConnectionSetting { return func(c *connection) { c.engine.Server() } }

// Accepter provides a function to be called when a connection receives an incoming
// request to open an endpoint, one of IncomingSession, IncomingSender or IncomingReceiver.
//
// The accept() function must not block or use the accepted endpoint.
// It can pass the endpoint to another goroutine for processing.
//
// By default all incoming endpoints are rejected.
func Accepter(accept func(Incoming)) ConnectionSetting {
	return func(c *connection) { c.accept = accept }
}

type connection struct {
	endpoint
	listenOnce, defaultSessionOnce, closeOnce sync.Once

	container   *container
	conn        net.Conn
	accept      func(Incoming)
	handler     *handler
	engine      *proton.Engine
	err         internal.ErrorHolder
	eConnection proton.Connection

	defaultSession Session
	done           chan struct{}
}

func newConnection(conn net.Conn, cont *container, setting ...ConnectionSetting) (*connection, error) {
	c := &connection{container: cont, conn: conn, accept: func(Incoming) {}, done: make(chan struct{})}
	c.handler = newHandler(c)
	var err error
	c.engine, err = proton.NewEngine(c.conn, c.handler.delegator)
	if err != nil {
		return nil, err
	}
	for _, set := range setting {
		set(c)
	}
	c.str = c.engine.String()
	c.eConnection = c.engine.Connection()
	go func() { c.engine.Run(); close(c.done) }()
	return c, nil
}

func (c *connection) Close(err error) { c.err.Set(err); c.engine.Close(err) }

func (c *connection) Disconnect(err error) { c.err.Set(err); c.engine.Disconnect(err) }

func (c *connection) Session(setting ...SessionSetting) (Session, error) {
	var s Session
	err := c.engine.InjectWait(func() error {
		eSession, err := c.engine.Connection().Session()
		if err == nil {
			eSession.Open()
			if err == nil {
				s = newSession(c, eSession, setting...)
			}
		}
		return err
	})
	return s, err
}

func (c *connection) Container() Container { return c.container }

func (c *connection) DefaultSession() (s Session, err error) {
	c.defaultSessionOnce.Do(func() {
		c.defaultSession, err = c.Session()
	})
	if err == nil {
		err = c.Error()
	}
	return c.defaultSession, err
}

func (c *connection) Sender(setting ...LinkSetting) (Sender, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Sender(setting...)
	} else {
		return nil, err
	}
}

func (c *connection) Receiver(setting ...LinkSetting) (Receiver, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Receiver(setting...)
	} else {
		return nil, err
	}
}

func (c *connection) Connection() Connection { return c }

func (c *connection) Wait() error { return c.WaitTimeout(Forever) }
func (c *connection) WaitTimeout(timeout time.Duration) error {
	_, err := timedReceive(c.done, timeout)
	if err == Timeout {
		return Timeout
	}
	return c.Error()
}

// Incoming is the interface for incoming requests to open an endpoint.
// Implementing types are IncomingSession, IncomingSender and IncomingReceiver.
type Incoming interface {
	// Accept the endpoint with default settings.
	//
	// You must not use the returned endpoint in the accept() function that
	// receives the Incoming value, but you can pass it to other goroutines.
	//
	// Implementing types provide type-specific Accept functions that take additional settings.
	Accept() Endpoint

	// Reject the endpoint with an error
	Reject(error)

	error() error
}

type incoming struct {
	err      error
	accepted bool
}

func (i *incoming) Reject(err error) { i.err = err }

func (i *incoming) error() error {
	switch {
	case i.err != nil:
		return i.err
	case !i.accepted:
		return amqp.Errorf(amqp.NotAllowed, "remote open rejected")
	default:
		return nil
	}
}
