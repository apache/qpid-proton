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

package proton

import (
	"fmt"
	"net"
	"os"
	"strings"
	"sync"
	"time"
	"unsafe"
)

/*
#include <proton/connection.h>
#include <proton/event.h>
#include <proton/error.h>
#include <proton/handlers.h>
#include <proton/session.h>
#include <proton/transport.h>
#include <memory.h>
#include <stdlib.h>
*/
import "C"

// Injecter allows functions to be "injected" into the event-processing loop, to
// be called in the same goroutine as event handlers.
type Injecter interface {
	// Inject a function into the engine goroutine.
	//
	// f() will be called in the same goroutine as event handlers, so it can safely
	// use values belonging to event handlers without synchronization. f() should
	// not block, no further events or injected functions can be processed until
	// f() returns.
	//
	// Returns a non-nil error if the function could not be injected and will
	// never be called. Otherwise the function will eventually be called.
	//
	// Note that proton values (Link, Session, Connection etc.) that existed when
	// Inject(f) was called may have become invalid by the time f() is executed.
	// Handlers should handle keep track of Closed events to ensure proton values
	// are not used after they become invalid. One technique is to have map from
	// proton values to application values. Check that the map has the correct
	// proton/application value pair at the start of the injected function and
	// delete the value from the map when handling a Closed event.
	Inject(f func()) error

	// InjectWait is like Inject but does not return till f() has completed.
	// If f() cannot be injected it returns the error from Inject(), otherwise
	// it returns the error from f()
	InjectWait(f func() error) error
}

// Engine reads from a net.Conn, decodes AMQP events and calls the appropriate
// Handler functions sequentially in a single goroutine. Actions taken by
// Handler functions (such as sending messages) are encoded and written to the
// net.Conn. You can create multiple Engines to handle multiple connections
// concurrently.
//
// You implement the EventHandler and/or MessagingHandler interfaces and provide
// those values to NewEngine(). Their HandleEvent method will be called in the
// event-handling goroutine.
//
// Handlers can pass values from an event (Connections, Links, Deliveries etc.) to
// other goroutines, store them, or use them as map indexes. Effectively they are
// just pointers.  Other goroutines cannot call their methods directly but they can
// can create a function closure to call such methods and pass it to Engine.Inject()
// to have it evaluated in the engine goroutine.
//
// You are responsible for ensuring you don't use an event value after it is
// invalid. The handler methods will tell you when a value is no longer valid. For
// example after a LinkClosed event, that link is no longer valid. If you do
// Link.Close() yourself (in a handler or injected function) the link remains valid
// until the corresponding LinkClosed event is received by the handler.
//
// Engine.Close() will take care of cleaning up any remaining values when you are
// done with the Engine. All values associated with a engine become invalid when you
// call Engine.Close()
//
// The qpid.apache.org/proton/concurrent package will do all this for you, so it
// may be a better choice for some applications.
//
type Engine struct {
	// Error is set on exit from Run() if there was an error.
	err    ErrorHolder
	inject chan func()

	conn             net.Conn
	connection       Connection
	transport        Transport
	collector        *C.pn_collector_t
	handlers         []EventHandler // Handlers for proton events.
	running          chan struct{}  // This channel will be closed when the goroutines are done.
	closeOnce        sync.Once
	timer            *time.Timer
	traceEvent       bool
	reading, writing bool
}

const bufferSize = 4096

func envBool(name string) bool {
	v := strings.ToLower(os.Getenv(name))
	return v == "true" || v == "1" || v == "yes" || v == "on"
}

// Create a new Engine and call Initialize() with conn and handlers
func NewEngine(conn net.Conn, handlers ...EventHandler) (*Engine, error) {
	eng := &Engine{}
	return eng, eng.Initialize(conn, handlers...)
}

// Initialize an Engine with a connection and handlers. Start it with Run()
func (eng *Engine) Initialize(conn net.Conn, handlers ...EventHandler) error {
	eng.inject = make(chan func())
	eng.conn = conn
	eng.connection = Connection{C.pn_connection()}
	eng.transport = Transport{C.pn_transport()}
	eng.collector = C.pn_collector()
	eng.handlers = handlers
	eng.running = make(chan struct{})
	eng.timer = time.NewTimer(0)
	eng.traceEvent = envBool("PN_TRACE_EVT")
	if eng.transport.IsNil() || eng.connection.IsNil() || eng.collector == nil {
		eng.free()
		return fmt.Errorf("proton.NewEngine cannot allocate")
	}
	C.pn_connection_collect(eng.connection.pn, eng.collector)
	return nil
}

// Create a byte slice backed by C memory.
// Empty or error (size <= 0) returns a nil byte slice.
func cByteSlice(start unsafe.Pointer, size int) []byte {
	if start == nil || size <= 0 {
		return nil
	} else {
		// Slice from very large imaginary array in C memory
		return (*[1 << 30]byte)(start)[:size:size]
	}
}

func (eng *Engine) Connection() Connection {
	return eng.connection
}

func (eng *Engine) Transport() Transport {
	return eng.transport
}

func (eng *Engine) String() string {
	return fmt.Sprintf("[%s]%s-%s", eng.Id(), eng.conn.LocalAddr(), eng.conn.RemoteAddr())
}

func (eng *Engine) Id() string {
	// Use transport address to match default PN_TRACE_FRM=1 output.
	return fmt.Sprintf("%p", eng.Transport().CPtr())
}

func (eng *Engine) Error() error {
	return eng.err.Get()
}

// Inject a function into the Engine's event loop.
//
// f() will be called in the same event-processing goroutine that calls Handler
// methods. f() can safely call methods on values that belong to this engine
// (Sessions, Links etc)
//
// The injected function has no parameters or return values. It is normally a
// closure and can use channels to communicate with the injecting goroutine if
// necessary.
//
// Returns a non-nil error if the engine is closed before the function could be
// injected.
func (eng *Engine) Inject(f func()) error {
	select {
	case eng.inject <- f:
		return nil
	case <-eng.running:
		return eng.Error()
	}
}

// InjectWait is like Inject but does not return till f() has completed or the
// engine is closed, and returns an error value from f()
func (eng *Engine) InjectWait(f func() error) error {
	done := make(chan error)
	defer close(done)
	err := eng.Inject(func() { done <- f() })
	if err != nil {
		return err
	}
	select {
	case <-eng.running:
		return eng.Error()
	case err := <-done:
		return err
	}
}

// Server puts the Engine in server mode, meaning it will auto-detect security settings on
// the incoming connection such as use of SASL and SSL.
// Must be called before Run()
//
func (eng *Engine) Server() { eng.Transport().SetServer() }

func (eng *Engine) disconnect(err error) {
	cond := eng.Transport().Condition()
	cond.SetError(err)              // Set the provided error.
	cond.SetError(eng.conn.Close()) // Use connection error if cond is not already set.
	eng.transport.CloseTail()
	eng.transport.CloseHead()
}

// Close the engine's connection.
// If err != nil pass it to the remote end as the close condition.
// Returns when the remote end closes or disconnects.
func (eng *Engine) Close(err error) {
	_ = eng.Inject(func() { CloseError(eng.Connection(), err) })
	<-eng.running
}

// CloseTimeout like Close but disconnect if the remote end doesn't close within timeout.
func (eng *Engine) CloseTimeout(err error, timeout time.Duration) {
	_ = eng.Inject(func() { CloseError(eng.Connection(), err) })
	select {
	case <-eng.running:
	case <-time.After(timeout):
		eng.Disconnect(err)
	}
}

// Disconnect the engine's connection immediately without an AMQP close.
// Process any termination events before returning.
func (eng *Engine) Disconnect(err error) {
	_ = eng.Inject(func() { eng.disconnect(err) })
	<-eng.running
}

// For debugging purposes: like Transport.Log() but takes a format string
// and works even if the transport has been freed.
func (eng *Engine) log(format string, args ...interface{}) {
	fmt.Fprintf(os.Stderr, "[%p]: %v", eng.transport, fmt.Sprintf(format, args...))
}

var processStart = time.Now() // Process start time for elapsed()

// Monotonic elapsed time since processStart
func elapsed() time.Duration {
	// Time.Sub() uses monotonic time.
	return time.Now().Sub(processStart)
}

// Let proton run timed activity and set up the next tick
func (eng *Engine) tick() {
	// Proton wants millisecond monotonic time
	now := int64(elapsed() / time.Millisecond)
	next := eng.Transport().Tick(now)
	if next != 0 {
		eng.timer.Reset(time.Duration((next - now) * int64(time.Millisecond)))
	}
}

func (eng *Engine) dispatch() bool {
	for ce := C.pn_collector_peek(eng.collector); ce != nil; ce = C.pn_collector_peek(eng.collector) {
		e := makeEvent(ce, eng)
		if eng.traceEvent {
			eng.transport.Log(e.String())
		}
		for _, h := range eng.handlers {
			h.HandleEvent(e)
		}
		if e.Type() == EConnectionRemoteOpen {
			eng.tick() // Update the tick if changed by remote.
		}
		C.pn_collector_pop(eng.collector)
	}
	return !eng.transport.Closed() || C.pn_collector_peek(eng.collector) != nil
}

func (eng *Engine) write() {
	if !eng.writing {
		size := eng.Transport().Pending() // Evaluate before Head(), may change buffer.
		start := eng.Transport().Head()
		if size > 0 {
			eng.writing = true
			go func() { // Blocking Write() in separate goroutineb
				n, err := eng.conn.Write(cByteSlice(start, size))
				eng.Inject(func() { // Inject results of Write back to engine goroutine
					eng.writing = false
					if n > 0 {
						eng.transport.Pop(uint(n))
					}
					if err != nil {
						eng.Transport().Condition().SetError(err)
						eng.Transport().CloseHead()
					}
				})
			}()
		}
	}
}

func (eng *Engine) read() {
	if !eng.reading {
		size := eng.Transport().Capacity()
		start := eng.Transport().Tail()
		if size > 0 {
			eng.reading = true
			go func() { // Blocking Read in separate goroutine
				n, err := eng.conn.Read(cByteSlice(start, size))
				eng.Inject(func() {
					eng.reading = false
					if n > 0 {
						eng.Transport().Process(uint(n))
					}
					if err != nil {
						eng.Transport().Condition().SetError(err)
						eng.Transport().CloseTail()
					}
				})
			}()
		}
	}
}

func (eng *Engine) free() {
	if !eng.transport.IsNil() {
		eng.transport.Unbind()
		eng.transport.Free()
		eng.transport = Transport{}
	}
	if !eng.connection.IsNil() {
		eng.connection.Free()
		eng.connection = Connection{}
	}
	if eng.collector != nil {
		C.pn_collector_release(eng.collector)
		C.pn_collector_free(eng.collector)
		eng.collector = nil
	}
}

// Run the engine. Engine.Run() will exit when the engine is closed or
// disconnected.  You can check for errors after exit with Engine.Error().
//
func (eng *Engine) Run() error {
	defer eng.free()
	eng.transport.Bind(eng.connection)
	eng.tick() // Start ticking if needed
	for eng.dispatch() {
		// Initiate read/write if needed
		eng.read()
		eng.write()
		select {
		case f := <-eng.inject: // User or IO action
			f()
		case <-eng.timer.C:
			eng.tick()
		}
	}
	eng.err.Set(EndpointError(eng.Connection()))
	eng.err.Set(eng.Transport().Condition().Error())
	close(eng.running)   // Signal goroutines have exited and Error is set, disable Inject()
	_ = eng.conn.Close() // Close conn, force read goroutine to exit (Inject will fail)
	return eng.err.Get()
}
