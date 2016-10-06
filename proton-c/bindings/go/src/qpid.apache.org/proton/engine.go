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
	"sync"
	"time"
	"unsafe"
)

/*
#include <proton/connection.h>
#include <proton/connection_engine.h>
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
// until the corresponing LinkClosed event is received by the handler.
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

	conn      net.Conn
	engine    C.pn_connection_engine_t
	handlers  []EventHandler // Handlers for proton events.
	running   chan struct{}  // This channel will be closed when the goroutines are done.
	closeOnce sync.Once
	timer     *time.Timer
}

const bufferSize = 4096

// NewEngine initializes a engine with a connection and handlers. To start it running:
//    eng := NewEngine(...)
//    go run eng.Run()
// The goroutine will exit when the engine is closed or disconnected.
// You can check for errors on Engine.Error.
//
func NewEngine(conn net.Conn, handlers ...EventHandler) (*Engine, error) {
	eng := &Engine{
		inject:   make(chan func()),
		conn:     conn,
		handlers: handlers,
		running:  make(chan struct{}),
		timer:    time.NewTimer(0),
	}
	if pnErr := C.pn_connection_engine_init(&eng.engine); pnErr != 0 {
		return nil, fmt.Errorf("cannot setup engine: %s", PnErrorCode(pnErr))
	}
	return eng, nil
}

// Create a byte slice backed by the memory of a pn_rwbytes_t, no copy.
// Empty buffer {0,0} returns a nil byte slice.
func byteSlice(data unsafe.Pointer, size C.size_t) []byte {
	if data == nil || size == 0 {
		return nil
	} else {
		return (*[1 << 30]byte)(data)[:size:size]
	}
}

func (eng *Engine) Connection() Connection {
	return Connection{C.pn_connection_engine_connection(&eng.engine)}
}

func (eng *Engine) Transport() Transport {
	return Transport{C.pn_connection_engine_transport(&eng.engine)}
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
// the incoming connnection such as use of SASL and SSL.
// Must be called before Run()
//
func (eng *Engine) Server() { eng.Transport().SetServer() }

func (eng *Engine) disconnect(err error) {
	cond := eng.Transport().Condition()
	cond.SetError(err)              // Set the provided error.
	cond.SetError(eng.conn.Close()) // Use connection error if cond is not already set.
	C.pn_connection_engine_disconnected(&eng.engine)
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

// Let proton run timed activity and set up the next tick
func (eng *Engine) tick() {
	now := time.Now()
	next := eng.Transport().Tick(now)
	if !next.IsZero() {
		eng.timer.Reset(next.Sub(now))
	}
}

func (eng *Engine) dispatch() bool {
	var needTick bool // Set if we need to tick the transport.
	for {
		cevent := C.pn_connection_engine_dispatch(&eng.engine)
		if cevent == nil {
			break
		}
		event := makeEvent(cevent, eng)
		if event.Type() == ETransport {
			needTick = true
		}
		for _, h := range eng.handlers {
			h.HandleEvent(event)
		}
	}
	if needTick {
		eng.tick()
	}
	return !bool(C.pn_connection_engine_finished(&eng.engine))
}

func (eng *Engine) writeBuffer() []byte {
	w := C.pn_connection_engine_write_buffer(&eng.engine)
	return byteSlice(unsafe.Pointer(w.start), w.size)
}

func (eng *Engine) readBuffer() []byte {
	r := C.pn_connection_engine_read_buffer(&eng.engine)
	return byteSlice(unsafe.Pointer(r.start), r.size)
}

// Run the engine. Engine.Run() will exit when the engine is closed or
// disconnected.  You can check for errors after exit with Engine.Error().
//
func (eng *Engine) Run() error {
	C.pn_connection_engine_start(&eng.engine)
	// Channels for read and write buffers going in and out of the read/write goroutines.
	// The channels are unbuffered: we want to exchange buffers in seuquence.
	readsIn, writesIn := make(chan []byte), make(chan []byte)
	readsOut, writesOut := make(chan []byte), make(chan []byte)

	wait := sync.WaitGroup{}
	wait.Add(2) // Read and write goroutines

	go func() { // Read goroutine
		defer wait.Done()
		for {
			rbuf, ok := <-readsIn
			if !ok {
				return
			}
			n, err := eng.conn.Read(rbuf)
			if n > 0 {
				readsOut <- rbuf[:n]
			} else if err != nil {
				_ = eng.Inject(func() {
					eng.Transport().Condition().SetError(err)
					C.pn_connection_engine_read_close(&eng.engine)
				})
				return
			}
		}
	}()

	go func() { // Write goroutine
		defer wait.Done()
		for {
			wbuf, ok := <-writesIn
			if !ok {
				return
			}
			n, err := eng.conn.Write(wbuf)
			if n > 0 {
				writesOut <- wbuf[:n]
			} else if err != nil {
				_ = eng.Inject(func() {
					eng.Transport().Condition().SetError(err)
					C.pn_connection_engine_write_close(&eng.engine)
				})
				return
			}
		}
	}()

	for eng.dispatch() {
		readBuf := eng.readBuffer()
		writeBuf := eng.writeBuffer()
		// Note that getting the buffers can generate events (eg. SASL events) that
		// might close the transport. Check if we are already finished before
		// blocking for IO.
		if !eng.dispatch() {
			break
		}

		// sendReads/sendWrites are nil (not sendable in select) unless we have a
		// buffer to read/write
		var sendReads, sendWrites chan []byte
		if readBuf != nil {
			sendReads = readsIn
		}
		if writeBuf != nil {
			sendWrites = writesIn
		}

		// Send buffers to the read/write goroutines if we have them.
		// Get buffers from the read/write goroutines and process them
		// Check for injected functions
		select {

		case sendReads <- readBuf:

		case sendWrites <- writeBuf:

		case buf := <-readsOut:
			C.pn_connection_engine_read_done(&eng.engine, C.size_t(len(buf)))

		case buf := <-writesOut:
			C.pn_connection_engine_write_done(&eng.engine, C.size_t(len(buf)))

		case f, ok := <-eng.inject: // Function injected from another goroutine
			if ok {
				f()
			}

		case <-eng.timer.C:
			eng.tick()
		}
	}

	eng.err.Set(EndpointError(eng.Connection()))
	eng.err.Set(eng.Transport().Condition().Error())
	close(readsIn)
	close(writesIn)
	close(eng.running)   // Signal goroutines have exited and Error is set, disable Inject()
	_ = eng.conn.Close() // Close conn, force read/write goroutines to exit (they will Inject)
	wait.Wait()          // Wait for goroutines

	for _, h := range eng.handlers {
		switch h := h.(type) {
		case cHandler:
			C.pn_handler_free(h.pn)
		}
	}
	C.pn_connection_engine_final(&eng.engine)
	return eng.err.Get()
}
