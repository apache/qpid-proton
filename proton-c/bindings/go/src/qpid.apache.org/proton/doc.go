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
Package proton provides a Go binding for the Qpid proton AMQP library.
AMQP is an open standard for inter-operable message exchange, see <http://www.amqp.org/>

Proton is an event-driven, concurrent-unsafe AMQP protocol library that allows
you to send and receive messages using the standard AMQP concurrent protocol.

For most tasks, consider using package `qpid.apache.org/proton/concurrent`.  It
provides a concurrent-safe API that is easier and more natural to use in Go.

The raw proton API is event-driven and not concurrent-safe. You implement a
MessagingHandler event handler to react to AMQP protocol events. You must ensure
that all events are handled in a single goroutine or that you serialize all all
uses of the proton objects associated with a single connection using a lock.
You can use channels to communicate between application goroutines and the
event-handling goroutine, see type Event fro more detail.

Package `qpid.apache.org/proton/concurrent` does all this for you and presents
a simple concurrent-safe interface.

*/
package proton

// #cgo LDFLAGS: -lqpid-proton
import "C"

// This file is just for the package comment.
