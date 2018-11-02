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
	"qpid.apache.org/proton"
	"sync"
	"time"
)

// Settings associated with a Connection.
type ConnectionSettings interface {
	// Authenticated user name associated with the connection.
	User() string

	// The AMQP virtual host name for the connection.
	//
	// Optional, useful when the server has multiple names and provides different
	// service based on the name the client uses to connect.
	//
	// By default it is set to the DNS host name that the client uses to connect,
	// but it can be set to something different at the client side with the
	// VirtualHost() option.
	//
	// Returns error if the connection fails to authenticate.
	VirtualHost() string

	// Heartbeat is the maximum delay between sending frames that the remote peer
	// has requested of us. If the interval expires an empty "heartbeat" frame
	// will be sent automatically to keep the connection open.
	Heartbeat() time.Duration
}

// Connection is an AMQP connection, created by a Container.
type Connection interface {
	Endpoint
	ConnectionSettings

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

	// Incoming returns a channel for incoming endpoints opened by the remote peer.
	// See the Incoming interface for more detail.
	//
	// Note: this channel will first return an *IncomingConnection for the
	// connection itself which allows you to look at security information and
	// decide whether to Accept() or Reject() the connection. Then it will return
	// *IncomingSession, *IncomingSender and *IncomingReceiver as they are opened
	// by the remote end.
	//
	// Note 2: you must receiving from Incoming() and call Accept/Reject to avoid
	// blocking electron event loop. Normally you would run a loop in a goroutine
	// to handle incoming types that interest and Accept() those that don't.
	Incoming() <-chan Incoming
}

type connectionSettings struct {
	user, virtualHost string
	heartbeat         time.Duration
}

func (c connectionSettings) User() string             { return c.user }
func (c connectionSettings) VirtualHost() string      { return c.virtualHost }
func (c connectionSettings) Heartbeat() time.Duration { return c.heartbeat }

// ConnectionOption can be passed when creating a connection to configure various options
type ConnectionOption func(*connection)

// User returns a ConnectionOption sets the user name for a connection
func User(user string) ConnectionOption {
	return func(c *connection) {
		c.user = user
		c.pConnection.SetUser(user)
	}
}

// VirtualHost returns a ConnectionOption to set the AMQP virtual host for the connection.
// Only applies to outbound client connection.
func VirtualHost(virtualHost string) ConnectionOption {
	return func(c *connection) {
		c.virtualHost = virtualHost
		c.pConnection.SetHostname(virtualHost)
	}
}

// Password returns a ConnectionOption to set the password used to establish a
// connection.  Only applies to outbound client connection.
//
// The connection will erase its copy of the password from memory as soon as it
// has been used to authenticate. If you are concerned about passwords staying in
// memory you should never store them as strings, and should overwrite your
// copy as soon as you are done with it.
//
func Password(password []byte) ConnectionOption {
	return func(c *connection) { c.pConnection.SetPassword(password) }
}

// Server returns a ConnectionOption to put the connection in server mode for incoming connections.
//
// A server connection will do protocol negotiation to accept a incoming AMQP
// connection. Normally you would call this for a connection created by
// net.Listener.Accept()
//
func Server() ConnectionOption {
	return func(c *connection) { c.engine.Server(); c.server = true; AllowIncoming()(c) }
}

// AllowIncoming returns a ConnectionOption to enable incoming endpoints, see
// Connection.Incoming() This is automatically set for Server() connections.
func AllowIncoming() ConnectionOption {
	return func(c *connection) { c.incoming = make(chan Incoming) }
}

// Parent returns a ConnectionOption that associates the Connection with it's Container
// If not set a connection will create its own default container.
func Parent(cont Container) ConnectionOption {
	return func(c *connection) { c.container = cont.(*container) }
}

type connection struct {
	endpoint
	connectionSettings

	defaultSessionOnce, closeOnce sync.Once

	container   *container
	conn        net.Conn
	server      bool
	incoming    chan Incoming
	handler     *handler
	engine      *proton.Engine
	pConnection proton.Connection

	defaultSession Session
}

// NewConnection creates a connection with the given options.
func NewConnection(conn net.Conn, opts ...ConnectionOption) (*connection, error) {
	c := &connection{
		conn: conn,
	}
	c.handler = newHandler(c)
	var err error
	c.engine, err = proton.NewEngine(c.conn, c.handler.delegator)
	if err != nil {
		return nil, err
	}
	c.pConnection = c.engine.Connection()
	for _, set := range opts {
		set(c)
	}
	if c.container == nil {
		c.container = NewContainer("").(*container)
	}
	c.pConnection.SetContainer(c.container.Id())
	saslConfig.setup(c.engine)
	c.endpoint.init(c.engine.String())
	go c.run()
	return c, nil
}

func (c *connection) run() {
	if !c.server {
		c.pConnection.Open()
	}
	_ = c.engine.Run()
	if c.incoming != nil {
		close(c.incoming)
	}
	_ = c.closed(Closed)
}

func (c *connection) Close(err error) {
	c.err.Set(err)
	c.engine.Close(err)
}

func (c *connection) Disconnect(err error) {
	c.err.Set(err)
	c.engine.Disconnect(err)
}

func (c *connection) Session(opts ...SessionOption) (Session, error) {
	var s Session
	err := c.engine.InjectWait(func() error {
		if c.Error() != nil {
			return c.Error()
		}
		pSession, err := c.engine.Connection().Session()
		if err == nil {
			pSession.Open()
			if err == nil {
				s = newSession(c, pSession, opts...)
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

func (c *connection) Sender(opts ...LinkOption) (Sender, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Sender(opts...)
	} else {
		return nil, err
	}
}

func (c *connection) Receiver(opts ...LinkOption) (Receiver, error) {
	if s, err := c.DefaultSession(); err == nil {
		return s.Receiver(opts...)
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

func (c *connection) Incoming() <-chan Incoming {
	assert(c.incoming != nil, "Incoming() is only allowed for a Connection created with the Server() option: %s", c)
	return c.incoming
}

type IncomingConnection struct {
	incoming
	connectionSettings
	c *connection
}

func newIncomingConnection(c *connection) *IncomingConnection {
	c.user = c.pConnection.Transport().User()
	c.virtualHost = c.pConnection.RemoteHostname()
	return &IncomingConnection{
		incoming:           makeIncoming(c.pConnection),
		connectionSettings: c.connectionSettings,
		c:                  c}
}

// AcceptConnection is like Accept() but takes ConnectionOption s
// For example you can set the Heartbeat() for the accepted connection.
func (in *IncomingConnection) AcceptConnection(opts ...ConnectionOption) Connection {
	return in.accept(func() Endpoint {
		for _, opt := range opts {
			opt(in.c)
		}
		in.c.pConnection.Open()
		return in.c
	}).(Connection)
}

func (in *IncomingConnection) Accept() Endpoint {
	return in.AcceptConnection()
}

func sasl(c *connection) proton.SASL { return c.engine.Transport().SASL() }

// SASLEnable returns a ConnectionOption that enables SASL authentication.
// Only required if you don't set any other SASL options.
func SASLEnable() ConnectionOption { return func(c *connection) { sasl(c) } }

// SASLAllowedMechs returns a ConnectionOption to set the list of allowed SASL
// mechanisms.
//
// Can be used on the client or the server to restrict the SASL for a connection.
// mechs is a space-separated list of mechanism names.
//
// The mechanisms allowed by default are determined by your SASL
// library and system configuration, with two exceptions: GSSAPI
// and GSS-SPNEGO are disabled by default.  To enable them, you
// must explicitly add them using this option.
//
// Clients must set the allowed mechanisms before the the
// outgoing connection is attempted.  Servers must set them
// before the listening connection is setup.
//
func SASLAllowedMechs(mechs string) ConnectionOption {
	return func(c *connection) { sasl(c).AllowedMechs(mechs) }
}

// SASLAllowInsecure returns a ConnectionOption that allows or disallows clear
// text SASL authentication mechanisms
//
// By default the SASL layer is configured not to allow mechanisms that disclose
// the clear text of the password over an unencrypted AMQP connection. This specifically
// will disallow the use of the PLAIN mechanism without using SSL encryption.
//
// This default is to avoid disclosing password information accidentally over an
// insecure network.
//
func SASLAllowInsecure(b bool) ConnectionOption {
	return func(c *connection) { sasl(c).SetAllowInsecureMechs(b) }
}

// Heartbeat returns a ConnectionOption that requests the maximum delay
// between sending frames for the remote peer. If we don't receive any frames
// within 2*delay we will close the connection.
//
func Heartbeat(delay time.Duration) ConnectionOption {
	// Proton-C divides the idle-timeout by 2 before sending, so compensate.
	return func(c *connection) { c.engine.Transport().SetIdleTimeout(2 * delay) }
}

type saslConfigState struct {
	lock        sync.Mutex
	name        string
	dir         string
	initialized bool
}

func (s *saslConfigState) set(target *string, value string) {
	s.lock.Lock()
	defer s.lock.Unlock()
	if s.initialized {
		panic("SASL configuration cannot be changed after a Connection has been created")
	}
	*target = value
}

// Apply the global SASL configuration the first time a proton.Engine needs it
//
// TODO aconway 2016-09-15: Current pn_sasl C impl config is broken, so all we
// can realistically offer is global configuration. Later if/when the pn_sasl C
// impl is fixed we can offer per connection over-rides.
func (s *saslConfigState) setup(eng *proton.Engine) {
	s.lock.Lock()
	defer s.lock.Unlock()
	if !s.initialized {
		s.initialized = true
		sasl := eng.Transport().SASL()
		if s.name != "" {
			sasl.ConfigName(saslConfig.name)
		}
		if s.dir != "" {
			sasl.ConfigPath(saslConfig.dir)
		}
	}
}

var saslConfig saslConfigState

// GlobalSASLConfigDir sets the SASL configuration directory for every
// Connection created in this process. If not called, the default is determined
// by your SASL installation.
//
// You can set SASLAllowInsecure and SASLAllowedMechs on individual connections.
//
// Must be called at most once, before any connections are created.
func GlobalSASLConfigDir(dir string) { saslConfig.set(&saslConfig.dir, dir) }

// GlobalSASLConfigName sets the SASL configuration name for every Connection
// created in this process. If not called the default is "proton-server".
//
// The complete configuration file name is
//     <sasl-config-dir>/<sasl-config-name>.conf
//
// You can set SASLAllowInsecure and SASLAllowedMechs on individual connections.
//
// Must be called at most once, before any connections are created.
func GlobalSASLConfigName(name string) { saslConfig.set(&saslConfig.name, name) }

// Do we support extended SASL negotiation?
// All implementations of Proton support ANONYMOUS and EXTERNAL on both
// client and server sides and PLAIN on the client side.
//
// Extended SASL implememtations use an external library (Cyrus SASL)
// to support other mechanisms beyond these basic ones.
func SASLExtended() bool { return proton.SASLExtended() }

// Dial is shorthand for using net.Dial() then NewConnection()
// See net.Dial() for the meaning of the network, address arguments.
func Dial(network, address string, opts ...ConnectionOption) (c Connection, err error) {
	conn, err := net.Dial(network, address)
	if err == nil {
		c, err = NewConnection(conn, opts...)
	}
	return
}

// DialWithDialer is shorthand for using dialer.Dial() then NewConnection()
// See net.Dial() for the meaning of the network, address arguments.
func DialWithDialer(dialer *net.Dialer, network, address string, opts ...ConnectionOption) (c Connection, err error) {
	conn, err := dialer.Dial(network, address)
	if err == nil {
		c, err = NewConnection(conn, opts...)
	}
	return
}
