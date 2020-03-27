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
Package proton wraps Proton-C, an event-driven, concurrent-unsafe AMQP 1.0
C library (package 'electron' is more "Go-like" and concurrent-safe)

This package requires the [proton-C library](http://qpid.apache.org/proton) to be installed.

Consult the C API documentation at http://qpid.apache.org/proton for more
information about the types here. There is a 1-1 correspondence between C type
pn_foo_t and Go type proton.Foo, and between C function

    pn_foo_do_something(pn_foo_t*, ...)

and Go method

    func (proton.Foo) DoSomething(...)

The proton.Engine type pumps data between a Go net.Conn and a proton event loop
goroutine that feeds events to a proton.MessagingHandler, which you must implement.
See the Engine documentation for more.

MessagingHandler defines an event handling interface that you can implement to
react to AMQP protocol events. There is also a lower-level EventHandler, but
MessagingHandler provides a simpler set of events and automates common tasks for you,
for most applications it will be more convenient.

NOTE: Methods on most types defined in this package (Sessions, Links etc.)  can
*only* be called in the event handler goroutine of the relevant
Connection/Engine, either by the HandleEvent method of a handler type or in a
function injected into the goroutine via Inject() or InjectWait() Handlers and
injected functions can set up channels to communicate with other goroutines.
Note the Injecter associated with a handler available as part of the Event value
passed to HandleEvent.

Separate Engine instances are independent, and can run concurrently.

The 'electron' package is built on the proton package but instead offers a
concurrent-safe API that can use simple procedural loops rather than event
handlers to express application logic. It is easier to use for most
applications.

*/
package proton

// #cgo LDFLAGS: -lqpid-proton-core
import "C"

// This file is just for the package comment.
