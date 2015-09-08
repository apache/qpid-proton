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

import (
	"io"
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/internal"
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
	// Open the endpoint.
	Open() error

	// Close an endpoint and signal an error to the remote end if error != nil.
	Close(error)

	// String is a human readable identifier, useful for debugging and logging.
	String() string

	// Error returns nil if the endpoint is open, otherwise returns an error.
	// Error() == Closed means the endpoint was closed without error.
	Error() error
}

// Implements setError() and Error() from Endpoint values that hold an error.
type errorHolder struct {
	err internal.FirstError
}

func (e *errorHolder) setError(err error) error { return e.err.Set(err) }
func (e *errorHolder) Error() error             { return e.err.Get() }

// Implements Error() and String() from Endpoint
type endpoint struct {
	errorHolder
	str string // Must be set by the value that embeds endpoint.
}

func (e *endpoint) String() string { return e.str }

// Call in proton goroutine to close an endpoint locally
// handler will complete the close when remote end closes.
func localClose(ep proton.Endpoint, err error) {
	if ep.State().LocalActive() {
		if err != nil {
			ep.Condition().SetError(err)
		}
		ep.Close()
	}
}

func (e *endpoint) closeError(err error) {
	if err == nil {
		err = Closed
	}
	e.err.Set(err)
}
