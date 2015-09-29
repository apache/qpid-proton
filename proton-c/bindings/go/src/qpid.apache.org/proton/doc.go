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

Package proton is a Go binding for the Qpid Proton AMQP messaging toolkit (see
http://qpid.apache.org/proton) It is a concurrent-unsafe, event-driven API that
closely follows the Proton C API.

Package qpid.apache.org/proton/concurrent provides an alternative,
concurrent-safe, procedural API. Most applications will find the concurrent API
easier to use.

If you need direct access to the underlying proton library for some reason, this
package provides it. The types in this package are simple wrappers for C
pointers. They provide access to C functions as Go methods and do some trivial
conversions, for example between Go string and C null-terminated char* strings.

Consult the C API documentation at http://qpid.apache.org/proton for more
information about the types here. There is a 1-1 correspondence between C type
pn_foo_t and Go type proton.Foo, and between C function

    pn_foo_do_something(pn_foo_t*, ...)

and Go method

    func (proton.Foo) DoSomething(...)

The proton.Engine type pumps data between a Go net.Conn connection and a
proton.Connection goroutine that feeds events to a proton.MessagingHandler. See
the proton.Engine documentation for more detail.

EventHandler and MessagingHandler define an event handling interfaces that you
can implement to react to protocol events. MessagingHandler provides a somewhat
simpler set of events and automates some common tasks for you.

You must ensure that all events are handled in a single goroutine or that you
serialize all all uses of the proton objects associated with a single connection
using a lock.  You can use channels to communicate between application
goroutines and the event-handling goroutine, see Engine documentation for more details.

Package qpid.apache.org/proton/concurrent does all this for you and presents a
simple concurrent-safe interface, for most applications you should use that
instead.

*/
package proton

// #cgo LDFLAGS: -lqpid-proton
import "C"

// This file is just for the package comment.
