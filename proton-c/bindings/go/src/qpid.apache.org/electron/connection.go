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
	"net"
	"qpid.apache.org/proton"
	"sync"
	"time"
)

// Connection is an AMQP connection, created by a Container.
type Connection interface {
	Endpoint

	// Sender opens a new sender on the DefaultSession.
	Sender(...LinkOption) (Sender, error)

	// Receiver opens a new Receiver on the DefaultSession().
	Receiver(...LinkOption) (Receiver, error)

	// DefaultSession() returns a default session for the connection. It is opened
	// on the first call to DefaultSession and returned on subsequent calls.
	DefaultSession() (Session, error)

	// Session opens a new session.
	Session(...SessionOption) (Session, error)

	// Container for the connection.
	Container() Container

	// Disconnect the connection abruptly with an error.
	Disconnect(error)

	// Wait waits for the connection to be disconnected.
	Wait() error

	// WaitTimeout is like Wait but returns Timeout if the timeout expires.
	WaitTimeout(time.Duration) error

	// Incoming returns a channel for incoming endpoints opened by the remote end.
	//
	// To enable, pass AllowIncoming() when creating the Connection. Otherwise all
	// incoming endpoint requests are automatically rejected and Incoming()
	// returns nil.
	//
	// An Incoming value can be an *IncomingSession, *IncomingSender or
	// *IncomingReceiver.  You must call Accept() to open the endpoint or Reject()
	// to close it with an error. The specific Incoming types have additional
	// methods to configure the endpoint.
	//
	// Not receiving from Incoming() or not calling Accept/Reject will block the
	// electron event loop. Normally you would have a dedicated goroutine receive
	// from Incoming() and start new goroutines to serve each incoming endpoint.
	// The channel is closed when the Connection closes.
	//
	Incoming() <-chan Incoming
}

// ConnectionOption can be passed when creating a connection to configure various options
type ConnectionOption func(*connection)

// Server returns a ConnectionOption to put the connection in server mode.
//
// A server connection will do protocol negotiation to accept a incoming AMQP
// connection. Normally you would call this for a connection created by
// net.Listener.Accept()
//
func Server() ConnectionOption { return func(c *connection) { c.engine.Server() } }

// AllowIncoming returns a ConnectionOption to enable incoming endpoint open requests.
// See Connection.Incoming()
func AllowIncoming() ConnectionOption {
	return func(c *connection) { c.incoming = make(chan Incoming) }
}

type connection struct {
	endpoint
	defaultSessionOnce, closeOnce sync.Once

	container   *container
	conn        net.Conn
	incoming    chan Incoming
	handler     *handler
	engine      *proton.Engine
	pConnection proton.Connection

	defaultSession Session
}

func newConnection(conn net.Conn, cont *container, setting ...ConnectionOption) (*connection, error) {
	c := &connection{container: cont, conn: conn}
	c.handler = newHandler(c)
	var err error
	c.engine, err = proton.NewEngine(c.conn, c.handler.delegator)
	if err != nil {
		return nil, err
	}
	for _, set := range setting {
		set(c)
	}
	c.endpoint.init(c.engine.String())
	c.pConnection = c.engine.Connection()
	go c.run()
	return c, nil
}

func (c *connection) run() {
	c.engine.Run()
	if c.incoming != nil {
		close(c.incoming)
	}
	c.closed(Closed)
}

func (c *connection) Close(err error) {
	c.err.Set(err)
	c.engine.Close(err)
}

func (c *connection) Disconnect(err error) {
	c.err.Set(err)
	c.engine.Disconnect(err)
}

func (c *connection) Session(setting ...SessionOption) (Session, error) {
	var s Session
	err := c.engine.InjectWait(func() error {
		if c.Error() != nil {
			return c.Error()
		}
		pSession, err := c.engine.Connection().Session()
		if err == nil {
			pSession.Open()
			if err == nil {
				s = newSession(c, pSession, setting...)
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

func (c *connection) Sender(setting ...LinkOption) (Sender, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Sender(setting...)
	} else {
		return nil, err
	}
}

func (c *connection) Receiver(setting ...LinkOption) (Receiver, error) {
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

func (c *connection) Incoming() <-chan Incoming { return c.incoming }

// Incoming is the interface for incoming requests to open an endpoint.
// Implementing types are IncomingSession, IncomingSender and IncomingReceiver.
type Incoming interface {
	// Accept and open the endpoint.
	Accept() Endpoint

	// Reject the endpoint with an error
	Reject(error)

	// wait for and call the accept function, call in proton goroutine.
	wait() error
	pEndpoint() proton.Endpoint
}

type incoming struct {
	pep      proton.Endpoint
	acceptCh chan func() error
}

func makeIncoming(e proton.Endpoint) incoming {
	return incoming{pep: e, acceptCh: make(chan func() error)}
}

func (in *incoming) String() string   { return fmt.Sprintf("%s: %s", in.pep.Type(), in.pep) }
func (in *incoming) Reject(err error) { in.acceptCh <- func() error { return err } }

// Call in proton goroutine, wait for and call the accept function fr
func (in *incoming) wait() error { return (<-in.acceptCh)() }

func (in *incoming) pEndpoint() proton.Endpoint { return in.pep }

// Called in app goroutine to send an accept function to proton and return the resulting endpoint.
func (in *incoming) accept(f func() Endpoint) Endpoint {
	done := make(chan Endpoint)
	in.acceptCh <- func() error {
		ep := f()
		done <- ep
		return nil
	}
	return <-done
}
