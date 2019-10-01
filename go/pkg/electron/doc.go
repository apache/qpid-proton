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

/*
Package electron lets you write concurrent AMQP 1.0 messaging clients and servers.

This package requires the [proton-C library](http://github.com/apache/qpid-proton/go/pkg/proton) to be installed.

Start by creating a Container with NewContainer. An AMQP Container represents a
single AMQP "application" and can contain client and server connections.

You can enable AMQP over any connection that implements the standard net.Conn
interface. Typically you can connect with net.Dial() or listen for server
connections with net.Listen.  Enable AMQP by passing the net.Conn to
Container.Connection().

AMQP allows bi-direction peer-to-peer message exchange as well as
client-to-broker. Messages are sent over "links". Each link is one-way and has a
Sender and Receiver end. Connection.Sender() and Connection.Receiver() open
links to Send() and Receive() messages. Connection.Incoming() lets you accept
incoming links opened by the remote peer. You can open and accept multiple links
in both directions on a single Connection.

Some of the documentation examples show client and server side by side in a
single program, in separate goroutines. This is only for example purposes, real
AMQP applications would run in separate processes on the network.

Some of the documentation examples show client and server side by side in a
single program, in separate goroutines. This is only for example purposes, real
AMQP applications would run in separate processes on the network.
*/
package electron

//#cgo LDFLAGS: -lqpid-proton-core
import "C"

// Just for package comment

/* DEVELOPER NOTES

There is a single proton.Engine per connection, each driving it's own event-loop goroutine,
and each with a 'handler'. Most state for a connection is maintained on the handler, and
only accessed in the event-loop goroutine, so no locks are required there.

The handler sets up channels as needed to get or send data from user goroutines
using electron types like Sender or Receiver.

Engine.Inject injects actions into the event loop from user goroutines. It is
important to check at the start of an injected function that required objects
are still valid, for example a link may be remotely closed between the time a
Sender function calls Inject and the time the injected function is execute by
the handler goroutine.

*/
