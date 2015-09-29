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
	"net"
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/internal"
	"sync"
)

// Connection is an AMQP connection, created by a Container.
type Connection interface {
	Endpoint

	// Sender opens a new sender on the DefaultSession.
	//
	// v can be a string, which is used as the Target address, or a SenderSettings
	// struct containing more details settings.
	Sender(v interface{}) (Sender, error)

	// Receiver opens a new Receiver on the DefaultSession().
	//
	// v can be a string, which is used as the
	// Source address, or a ReceiverSettings struct containing more details
	// settings.
	Receiver(v interface{}) (Receiver, error)

	// Server puts the connection in server mode, must be called before Open().
	//
	// A server connection will do protocol negotiation to accept a incoming AMQP
	// connection. Normally you would call this for a connection accepted by
	// net.Listener.Accept()
	//
	Server()

	// Listen enables endpoints opened by the remote peer to be accepted by calling Accept().
	// Must be called before Open().
	Listen()

	// DefaultSession() returns a default session for the connection. It is opened
	// on the first call to DefaultSession and returned on subsequent calls.
	DefaultSession() (Session, error)

	// Session opens a new session.
	Session() (Session, error)

	// Accept returns the next Endpoint (Session, Sender or Receiver) opened by
	// the remote peer. It returns (nil, error) if the connection closes.
	//
	// You must call Endpoint.Open() to complete opening the returned Endpoint or
	// Endpoint.Close(error) to reject it. You can set endpoint properties before
	// calling Open()
	//
	// You can use a type switch or type conversion to test which kind of Endpoint
	// has been returned.
	//
	// You must call Connection.Listen() before Connection.Open() to enable Accept.
	//
	// The connection buffers endpoints until you call Accept() so normally you
	// should have a dedicated goroutine calling Accept() in a loop to process it
	// rapidly.
	//
	Accept() (Endpoint, error)

	// Container for the connection.
	Container() Container

	// Disconnect the connection abruptly with an error.
	Disconnect(error)
}

type connection struct {
	endpoint
	listenOnce, defaultSessionOnce sync.Once

	// Set before Open()
	container *container
	conn      net.Conn
	incoming  *internal.FlexChannel

	// Set by Open()
	handler     *handler
	engine      *proton.Engine
	err         internal.FirstError
	eConnection proton.Connection

	defaultSession Session
}

func newConnection(conn net.Conn, cont *container) (*connection, error) {
	c := &connection{container: cont, conn: conn}
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

func (c *connection) Listen() {
	c.listenOnce.Do(func() { c.incoming = internal.NewFlexChannel(-1) })
}

func (c *connection) Open() error {
	go c.engine.Run()
	return nil
}

func (c *connection) Close(err error) {
	c.engine.Close(err)
	c.setError(c.engine.Error()) // Will be io.EOF on close OK
	if c.incoming != nil {
		close(c.incoming.In)
	}
}

func (c *connection) Disconnect(err error) {
	c.engine.Disconnect(err)
	if c.incoming != nil {
		close(c.incoming.In)
	}
}

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

func (c *connection) handleIncoming(sn *session, l proton.Link) {
	if l.IsReceiver() {
		c.incoming.In <- newReceiver(makeIncomingLink(sn, l))
	} else {
		c.incoming.In <- newSender(makeIncomingLink(sn, l))
	}
}

func (c *connection) Accept() (Endpoint, error) {
	v, ok := <-c.incoming.Out
	if !ok {
		return nil, c.Error()
	} else {
		return v.(Endpoint), nil
	}
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

func (c *connection) Sender(v interface{}) (Sender, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Sender(v)
	} else {
		return nil, err
	}
}

func (c *connection) Receiver(v interface{}) (Receiver, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Receiver(v)
	} else {
		return nil, err
	}
}
