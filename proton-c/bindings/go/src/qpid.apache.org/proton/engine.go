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

// #include <proton/connection.h>
// #include <proton/event.h>
// #include <proton/handlers.h>
// #include <proton/session.h>
// #include <proton/transport.h>
// #include <memory.h>
// #include <stdlib.h>
//
// PN_HANDLE(REMOTE_ADDR)
import "C"

import (
	"fmt"
	"io"
	"net"
	"qpid.apache.org/internal"
	"sync"
	"unsafe"
)

// Injecter allows functions to be "injected" into an event-processing loop.
type Injecter interface {
	// Inject a function into an event-loop concurrency context.
	//
	// f() will be called in the same concurrency context as event handers, so it
	// can safely use values that can used be used in that context. If f blocks it
	// will block the event loop so be careful calling blocking functions in f.
	//
	// Returns a non-nil error if the function could not be injected.
	Inject(f func()) error

	// InjectWait is like Inject but does not return till f() has completed.
	// If f() cannot be injected it returns the error from Inject(), otherwise
	// it returns the error from f()
	InjectWait(f func() error) error
}

// bufferChan manages a pair of ping-pong buffers to pass bytes through a channel.
type bufferChan struct {
	buffers    chan []byte
	buf1, buf2 []byte
}

func newBufferChan(size int) *bufferChan {
	return &bufferChan{make(chan []byte), make([]byte, size), make([]byte, size)}
}

func (b *bufferChan) buffer() []byte {
	b.buf1, b.buf2 = b.buf2, b.buf1 // Alternate buffers.
	return b.buf1[:cap(b.buf1)]
}

// Engine reads from a net.Conn, decodes AMQP events and calls the appropriate
// Handler functions in a single event-loop goroutine. Actions taken by Handler
// functions (such as sending messages) are encoded and written to the
// net.Conn. Create a engine with NewEngine()
//
// The Engine runs a proton event loop in the goroutine that calls Engine.Run()
// and creates goroutines to feed data to/from a net.Conn. You can create
// multiple Engines to handle multiple connections concurrently.
//
// Methods on proton values defined in this package (Sessions, Links etc.) can
// only be called in the goroutine that executes the corresponding
// Engine.Run(). You implement the EventHandler or MessagingHandler interfaces
// and provide those values to NewEngine(). Their HandleEvent method will be
// called in the Engine goroutine, in typical event-driven style.
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
	err    internal.ErrorHolder
	inject chan func()

	conn       net.Conn
	connection Connection
	transport  Transport
	collector  *C.pn_collector_t
	read       *bufferChan    // Read buffers channel.
	write      *bufferChan    // Write buffers channel.
	handlers   []EventHandler // Handlers for proton events.
	running    chan struct{}  // This channel will be closed when the goroutines are done.
	closeOnce  sync.Once
}

const bufferSize = 4096

// Map of Connection to *Engine
var engines = internal.MakeSafeMap()

// NewEngine initializes a engine with a connection and handlers. To start it running:
//    eng := NewEngine(...)
//    go run eng.Run()
// The goroutine will exit when the engine is closed or disconnected.
// You can check for errors on Engine.Error.
//
func NewEngine(conn net.Conn, handlers ...EventHandler) (*Engine, error) {
	// Save the connection ID for Connection.String()
	eng := &Engine{
		inject:     make(chan func()),
		conn:       conn,
		transport:  Transport{C.pn_transport()},
		connection: Connection{C.pn_connection()},
		collector:  C.pn_collector(),
		handlers:   handlers,
		read:       newBufferChan(bufferSize),
		write:      newBufferChan(bufferSize),
		running:    make(chan struct{}),
	}
	if eng.transport.IsNil() || eng.connection.IsNil() || eng.collector == nil {
		return nil, internal.Errorf("failed to allocate engine")
	}

	// TODO aconway 2015-06-25: connection settings for user, password, container etc.
	// before transport.Bind() Set up connection before Engine, allow Engine or Reactor
	// to run connection.

	// Unique container-id by default.
	eng.connection.SetContainer(internal.UUID4().String())
	pnErr := eng.transport.Bind(eng.connection)
	if pnErr != 0 {
		return nil, internal.Errorf("cannot setup engine: %s", internal.PnErrorCode(pnErr))
	}
	C.pn_connection_collect(eng.connection.pn, eng.collector)
	eng.connection.Open()
	connectionContexts.Put(eng.connection, connectionContext{eng, eng.String()})
	return eng, nil
}

func (eng *Engine) String() string {
	return fmt.Sprintf("%s-%s", eng.conn.LocalAddr(), eng.conn.RemoteAddr())
}

func (eng *Engine) Id() string {
	return fmt.Sprintf("%eng", &eng)
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
func (eng *Engine) Server() { eng.transport.SetServer() }

// Close the engine's connection, returns when the engine has exited.
func (eng *Engine) Close(err error) {
	eng.Inject(func() {
		CloseError(eng.connection, err)
	})
	<-eng.running
}

// Disconnect the engine's connection without and AMQP close, returns when the engine has exited.
func (eng *Engine) Disconnect(err error) {
	if err != nil {
		eng.err.Set(err)
	}
	eng.conn.Close()
	<-eng.running
}

// Run the engine. Engine.Run() will exit when the engine is closed or
// disconnected.  You can check for errors after exit with Engine.Error().
//
func (eng *Engine) Run() error {
	wait := sync.WaitGroup{}
	wait.Add(2) // Read and write goroutines

	readErr := make(chan error, 1) // Don't block
	go func() {                    // Read goroutine
		defer wait.Done()
		for {
			rbuf := eng.read.buffer()
			n, err := eng.conn.Read(rbuf)
			if n > 0 {
				eng.read.buffers <- rbuf[:n]
			}
			if err != nil {
				readErr <- err
				close(readErr)
				close(eng.read.buffers)
				return
			}
		}
	}()

	writeErr := make(chan error, 1) // Don't block
	go func() {                     // Write goroutine
		defer wait.Done()
		for {
			wbuf, ok := <-eng.write.buffers
			if !ok {
				return
			}
			_, err := eng.conn.Write(wbuf)
			if err != nil {
				writeErr <- err
				close(writeErr)
				return
			}
		}
	}()

	wbuf := eng.write.buffer()[:0]
loop:
	for {
		if len(wbuf) == 0 {
			eng.pop(&wbuf)
		}
		// Don't set wchan unless there is something to write.
		var wchan chan []byte
		if len(wbuf) > 0 {
			wchan = eng.write.buffers
		}

		select {
		case buf, ok := <-eng.read.buffers: // Read a buffer
			if ok {
				eng.push(buf)
			}
		case wchan <- wbuf: // Write a buffer
			wbuf = eng.write.buffer()[:0]
		case f, ok := <-eng.inject: // Function injected from another goroutine
			if ok {
				f()
			}
		case err := <-readErr:
			eng.netError(err)
		case err := <-writeErr:
			eng.netError(err)
		}
		eng.process()
		if eng.err.Get() != nil {
			break loop
		}
	}
	close(eng.write.buffers)
	eng.conn.Close() // Make sure connection is closed
	wait.Wait()
	connectionContexts.Delete(eng.connection)
	if !eng.connection.IsNil() {
		eng.connection.Free()
	}
	if !eng.transport.IsNil() {
		eng.transport.Free()
	}
	if eng.collector != nil {
		C.pn_collector_free(eng.collector)
	}
	for _, h := range eng.handlers {
		switch h := h.(type) {
		case cHandler:
			C.pn_handler_free(h.pn)
		}
	}
	close(eng.running) // Signal goroutines have exited and Error is set.
	return eng.err.Get()
}

func (eng *Engine) netError(err error) {
	if err == nil {
		err = internal.Errorf("unknown network error")
	}
	eng.conn.Close() // Make sure both sides are closed
	eng.err.Set(err)
	eng.transport.CloseHead()
	eng.transport.CloseTail()
}

func minInt(a, b int) int {
	if a < b {
		return a
	} else {
		return b
	}
}

func (eng *Engine) pop(buf *[]byte) {
	pending := int(eng.transport.Pending())
	switch {
	case pending == int(C.PN_EOS):
		*buf = (*buf)[:]
		return
	case pending < 0:
		panic(internal.Errorf("%s", internal.PnErrorCode(pending)))
	}
	size := minInt(pending, cap(*buf))
	*buf = (*buf)[:size]
	if size == 0 {
		return
	}
	C.memcpy(unsafe.Pointer(&(*buf)[0]), eng.transport.Head(), C.size_t(size))
	internal.Assert(size > 0)
	eng.transport.Pop(uint(size))
}

func (eng *Engine) push(buf []byte) {
	buf2 := buf
	for len(buf2) > 0 {
		n := eng.transport.Push(buf2)
		if n <= 0 {
			panic(internal.Errorf("error in transport: %s", internal.PnErrorCode(n)))
		}
		buf2 = buf2[n:]
	}
}

func (eng *Engine) handle(e Event) {
	for _, h := range eng.handlers {
		h.HandleEvent(e)
	}
	if e.Type() == ETransportClosed {
		eng.err.Set(e.Connection().RemoteCondition().Error())
		eng.err.Set(e.Connection().Transport().Condition().Error())
		if eng.err.Get() == nil {
			eng.err.Set(io.EOF)
		}
	}
}

func (eng *Engine) process() {
	for ce := C.pn_collector_peek(eng.collector); ce != nil; ce = C.pn_collector_peek(eng.collector) {
		eng.handle(makeEvent(ce))
		C.pn_collector_pop(eng.collector)
	}
}

func (eng *Engine) Connection() Connection { return eng.connection }
