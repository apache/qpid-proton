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

Package concurrent provides a procedural, concurrent Go API for exchanging AMQP
messages. You can write clients or servers using this API.

Start by creating a Container with NewContainer. A Container represents a client
or server application that can contain incoming or outgoing connections.

You can create connections with the standard Go 'net' package using net.Dial or
net.Listen. Create an AMQP connection over a net.Conn with
Container.Connection() and open it with Connection.Open().

AMQP sends messages over "links", each link has a Sender and Receiver
end. Connection.Sender() and Connection.Receiver() allow you to create links to
send and receive messages.

You can also create an AMQP server connection by calling Connection.Listen()
before calling Open() on the connection. You can then call Connection.Accept()
after calling Connection.Open() to accept incoming sessions and links.

*/
package concurrent

//#cgo LDFLAGS: -lqpid-proton
import "C"

// Just for package comment
