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

import (
	"io"
	"qpid.apache.org/proton"
)

// Closed is an alias for io.EOF. It is returned as an error when an endpoint
// was closed cleanly.
var Closed = io.EOF

// Endpoint is the common interface for Connection, Session, Link, Sender and Receiver.
//
// Endpoints can be created locally or by the remote peer. You must Open() an
// endpoint before you can use it. Some endpoints have additional Set*() methods
// that must be called before Open() to take effect, see Connection, Session,
// Link, Sender and Receiver for details.
//
type Endpoint interface {
	// Close an endpoint and signal an error to the remote end if error != nil.
	Close(error)

	// String is a human readable identifier, useful for debugging and logging.
	String() string

	// Error returns nil if the endpoint is open, otherwise returns an error.
	// Error() == Closed means the endpoint was closed without error.
	Error() error

	// Connection containing the endpoint
	Connection() Connection

	// Done returns a channel that will close when the endpoint closes.
	// Error() will contain the reason.
	Done() <-chan struct{}

	// Called in handler goroutine when endpoint is remotely closed.
	closed(err error) error
}

// DEVELOPER NOTES
//
// An electron.Endpoint corresponds to a proton.Endpoint, which can be invalidated
//
type endpoint struct {
	err  proton.ErrorHolder
	str  string // Must be set by the value that embeds endpoint.
	done chan struct{}
}

func (e *endpoint) init(s string) { e.str = s; e.done = make(chan struct{}) }

// Called in handler on a Closed event. Marks the endpoint as closed and the corresponding
// proton.Endpoint pointer as invalid. Injected functions should check Error() to ensure
// the pointer has not been invalidated.
//
// Returns the error stored on the endpoint, which may not be different to err if there was
// already a n error
func (e *endpoint) closed(err error) error {
	select {
	case <-e.done:
		// Already closed
	default:
		e.err.Set(err)
		e.err.Set(Closed)
		close(e.done)
	}
	return e.err.Get()
}

func (e *endpoint) String() string { return e.str }

func (e *endpoint) Error() error { return e.err.Get() }

func (e *endpoint) Done() <-chan struct{} { return e.done }

// Call in proton goroutine to initiate closing an endpoint locally
// handler will complete the close when remote end closes.
func localClose(ep proton.Endpoint, err error) {
	if ep.State().LocalActive() {
		proton.CloseError(ep, err)
	}
}
