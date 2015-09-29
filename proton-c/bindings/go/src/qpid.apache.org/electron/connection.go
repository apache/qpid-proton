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
	"qpid.apache.org/internal"
	"qpid.apache.org/proton"
	"qpid.apache.org/amqp"
	"sync"
)

// Connection is an AMQP connection, created by a Container.
type Connection interface {
	Endpoint

	// Server puts the connection in server mode, must be called before Open().
	//
	// A server connection will do protocol negotiation to accept a incoming AMQP
	// connection. Normally you would call this for a connection created by
	// net.Listener.Accept()
	//
	Server()

	// Listen arranges for endpoints opened by the remote peer to be passed to accept().
	// Listen() must be called before Connection.Open().
	//
	// accept() is passed a Session, Sender or Receiver.  It can examine endpoint
	// properties and set some properties (e.g. Receiver.SetCapacity()) Returning nil
	// will accept the endpoint, returning an error will reject it.
	//
	// accept() must not block or use the endpoint other than to examine or set
	// properties.  It can start a goroutine to process the Endpoint, or pass the
	// Endpoint to another goroutine via a channel, and that goroutine can use
	// the endpoint as normal.
	//
	// The default Listen function is RejectEndpoint which rejects all endpoints.
	// You can call Listen(AcceptEndpoint) to accept all endpoints
	Listen(accept func(Endpoint) error)

	// Open the connection, ready for use.
	Open() error

	// Sender opens a new sender on the DefaultSession.
	//
	// v can be a string, which is used as the Target address, or a SenderSettings
	// struct containing more details settings.
	Sender(setting ...LinkSetting) (Sender, error)

	// Receiver opens a new Receiver on the DefaultSession().
	//
	// v can be a string, which is used as the
	// Source address, or a ReceiverSettings struct containing more details
	// settings.
	Receiver(setting ...LinkSetting) (Receiver, error)

	// DefaultSession() returns a default session for the connection. It is opened
	// on the first call to DefaultSession and returned on subsequent calls.
	DefaultSession() (Session, error)

	// Session opens a new session.
	Session() (Session, error)

	// Container for the connection.
	Container() Container

	// Disconnect the connection abruptly with an error.
	Disconnect(error)
}

// AcceptEndpoint pass to Connection.Listen to accept all endpoints
func AcceptEndpoint(Endpoint) error { return nil }

// RejectEndpoint pass to Connection.Listen to reject all endpoints
func RejectEndpoint(Endpoint) error {
	return amqp.Errorf(amqp.NotAllowed, "remote open rejected")
}

type connection struct {
	endpoint
	listenOnce, defaultSessionOnce, closeOnce sync.Once

	// Set before Open()
	container *container
	conn      net.Conn
	accept    func(Endpoint) error

	// Set by Open()
	handler     *handler
	engine      *proton.Engine
	err         internal.ErrorHolder
	eConnection proton.Connection

	defaultSession Session
}

func newConnection(conn net.Conn, cont *container) (*connection, error) {
	c := &connection{container: cont, conn: conn, accept: RejectEndpoint}
	c.handler = newHandler(c)
	var err error
	c.engine, err = proton.NewEngine(c.conn, c.handler.delegator)
	if err != nil {
		return nil, err
	}
	c.str = c.engine.String()
	c.eConnection = c.engine.Connection()
	return c, nil
}

func (c *connection) Server() { c.engine.Server() }

func (c *connection) Listen(accept func(Endpoint) error) { c.accept = accept }

func (c *connection) Open() error {
	go c.engine.Run()
	return nil
}

func (c *connection) Close(err error) { c.engine.Close(err) }

func (c *connection) Disconnect(err error) { c.engine.Disconnect(err) }

// FIXME aconway 2015-10-07:
func (c *connection) closed(err error) {
	// Call from another goroutine to initiate close without deadlock.
	go c.Close(err)
}

func (c *connection) Session() (Session, error) {
	var s Session
	err := c.engine.InjectWait(func() error {
		eSession, err := c.engine.Connection().Session()
		if err == nil {
			eSession.Open()
			if err == nil {
				s = newSession(c, eSession)
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
