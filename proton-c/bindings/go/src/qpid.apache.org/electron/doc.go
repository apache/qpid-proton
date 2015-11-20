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
Package electron is a procedural, concurrent-safe Go library for AMQP messaging.
You can write clients and servers using this library.

Start by creating a Container with NewContainer. A Container represents a client
or server application that can contain many incoming or outgoing connections.

Create connections with the standard Go 'net' package using net.Dial or
net.Listen. Create an AMQP connection over a net.Conn with
Container.Connection() and open it with Connection.Open().

AMQP sends messages over "links". Each link has a Sender end and a Receiver
end. Connection.Sender() and Connection.Receiver() allow you to create links to
Send() and Receive() messages.

You can create an AMQP server connection by calling Connection.Server() and
Connection.Listen() before calling Connection.Open(). A server connection can
negotiate protocol security details and can accept incoming links opened from
the remote end of the connection.

*/
package electron

//#cgo LDFLAGS: -lqpid-proton
import "C"

// Just for package comment

/* DEVELOPER NOTES

There is a single proton.Engine per connection, each driving it's own event-loop goroutine,
and each with a 'handler'. Most state for a connection is maintained on the handler, and
only accessed in the event-loop goroutine, so no locks are required there.

The handler sets up channels as needed to get or send data from user goroutines
using electron types like Sender or Receiver.

We also use Engine.Inject to inject actions into the event loop from user
goroutines. It is important to check at the start of an injected function that
required objects are still valid, for example a link may be remotely closed
between the time a Sender function calls Inject and the time the injected
function is execute by the handler goroutine. See comments in endpoint.go for more.

*/
