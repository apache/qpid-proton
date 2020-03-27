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
	"fmt"
	"io"
	"github.com/apache/qpid-proton/go/pkg/proton"
)

// Closed is an alias for io.EOF. It is returned as an error when an endpoint
// was closed cleanly.
var Closed = io.EOF

// EOF is an alias for io.EOF. It is returned as an error when an endpoint
// was closed cleanly.
var EOF = io.EOF

// Endpoint is the local end of a communications channel to the remote peer
// process.  The following interface implement Endpoint: Connection, Session,
// Sender and Receiver.
//
// You can create an endpoint with functions on Container, Connection and
// Session. You can accept incoming endpoints from the remote peer using
// Connection.Incoming()
//
type Endpoint interface {
	// Close an endpoint and signal an error to the remote end if error != nil.
	Close(error)

	// String is a human readable identifier, useful for debugging and logging.
	String() string

	// Error returns nil if the endpoint is open, otherwise returns an error.
	// Error() == Closed means the endpoint was closed without error.
	Error() error

	// Connection is the connection associated with this endpoint.
	Connection() Connection

	// Done returns a channel that will close when the endpoint closes.
	// After Done() has closed, Error() will return the reason for closing.
	Done() <-chan struct{}

	// Sync() waits for the remote peer to confirm the endpoint is active or
	// reject it with an error. You can call it immediately on new endpoints
	// for more predictable error handling.
	//
	// AMQP is an asynchronous protocol. It is legal to create an endpoint and
	// start using it without waiting for confirmation. This avoids a needless
	// delay in the non-error case and throughput by "assuming the best".
	//
	// However if there *is* an error, these "optimistic" actions will fail. The
	// endpoint and its children will be closed with an error. The error will only
	// be detected when you try to use one of these endpoints or call Sync()
	Sync() error
}

type endpointInternal interface {
	// Called in handler goroutine when endpoint is remotely closed.
	closed(err error) error
	wakeSync()
}

// Base implementation for Endpoint
type endpoint struct {
	err    proton.ErrorHolder
	str    string // String() return value.
	done   chan struct{}
	active chan struct{}
}

func (e *endpoint) init(s string) {
	e.str = s
	e.done = make(chan struct{})
	e.active = make(chan struct{})
}

// Called in proton goroutine on remote open.
func (e *endpoint) wakeSync() {
	select { // Close active channel if not already closed.
	case <-e.active:
	default:
		close(e.active)
	}
}

// Called in proton goroutine (from handler) on a Closed or Disconnected event.
//
// Set err if there is not already an error on the endpoint.
// Return Error()
func (e *endpoint) closed(err error) error {
	select {
	case <-e.done:
		// Already closed
	default:
		e.err.Set(err)
		e.err.Set(Closed)
		e.wakeSync() // Make sure we wake up Sync()
		close(e.done)
	}
	return e.Error()
}

func (e *endpoint) String() string { return e.str }

func (e *endpoint) Error() error { return e.err.Get() }

func (e *endpoint) Done() <-chan struct{} { return e.done }

func (e *endpoint) Sync() error {
	<-e.active
	return e.Error()
}

// Call in proton goroutine to initiate closing an endpoint locally
// handler will complete the close when remote end closes.
func localClose(ep proton.Endpoint, err error) {
	if ep.State().LocalActive() {
		proton.CloseError(ep, err)
	}
}

// Incoming is the interface for incoming endpoints, see Connection.Incoming()
//
// Call Incoming.Accept() to open the endpoint or Incoming.Reject() to close it
// with optional error
//
// Implementing types are *IncomingConnection, *IncomingSession, *IncomingSender
// and *IncomingReceiver. Each type provides methods to examine the incoming
// endpoint request and set configuration options for the local endpoint
// before calling Accept() or Reject()
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

// Call in proton goroutine, wait for and call the accept function.
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
