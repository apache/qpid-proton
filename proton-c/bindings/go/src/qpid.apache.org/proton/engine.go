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
	"qpid.apache.org/proton/internal"
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
// The proton protocol engine is single threaded (per connection). The Engine runs
// proton in the goroutine that calls Engine.Run() and creates goroutines to feed
// data to/from a net.Conn. You can create multiple Engines to handle multiple
// connections concurrently.
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
	err    internal.FirstError
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
//    p := NewEngine(...)
//    go run p.Run()
// The goroutine will exit when the engine is closed or disconnected.
// You can check for errors on Engine.Error.
//
func NewEngine(conn net.Conn, handlers ...EventHandler) (*Engine, error) {
	// Save the connection ID for Connection.String()
	p := &Engine{
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
	if p.transport.IsNil() || p.connection.IsNil() || p.collector == nil {
		return nil, internal.Errorf("failed to allocate engine")
	}

	// TODO aconway 2015-06-25: connection settings for user, password, container etc.
	// before transport.Bind() Set up connection before Engine, allow Engine or Reactor
	// to run connection.

	// Unique container-id by default.
	p.connection.SetContainer(internal.UUID4().String())
	pnErr := p.transport.Bind(p.connection)
	if pnErr != 0 {
		return nil, internal.Errorf("cannot setup engine: %s", internal.PnErrorCode(pnErr))
	}
	C.pn_connection_collect(p.connection.pn, p.collector)
	p.connection.Open()
	connectionContexts.Put(p.connection, connectionContext{p, p.String()})
	return p, nil
}

func (p *Engine) String() string {
	return fmt.Sprintf("%s-%s", p.conn.LocalAddr(), p.conn.RemoteAddr())
}

func (p *Engine) Id() string {
	return fmt.Sprintf("%p", &p)
}

func (p *Engine) Error() error {
	return p.err.Get()
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
func (p *Engine) Inject(f func()) error {
	select {
	case p.inject <- f:
		return nil
	case <-p.running:
		return p.Error()
	}
}

// InjectWait is like Inject but does not return till f() has completed or the
// engine is closed, and returns an error value from f()
func (p *Engine) InjectWait(f func() error) error {
	done := make(chan error)
	defer close(done)
	err := p.Inject(func() { done <- f() })
	if err != nil {
		return err
	}
	select {
	case <-p.running:
		return p.Error()
	case err := <-done:
		return err
	}
}

// Server puts the Engine in server mode, meaning it will auto-detect security settings on
// the incoming connnection such as use of SASL and SSL.
// Must be called before Run()
//
func (p *Engine) Server() { p.transport.SetServer() }

// Close the engine's connection, returns when the engine has exited.
func (p *Engine) Close(err error) {
	p.Inject(func() {
		if err != nil {
			p.connection.Condition().SetError(err)
		}
		p.connection.Close()
	})
	<-p.running
}

// Disconnect the engine's connection without and AMQP close, returns when the engine has exited.
func (p *Engine) Disconnect(err error) {
	if err != nil {
		p.err.Set(err)
	}
	p.conn.Close()
	<-p.running
}

// Run the engine. Normally called in a goroutine as: go engine.Run()
// Engine.Run() will exit when the engine is closed or disconnected.
// You can check for errors after exit with Engine.Error().
//
func (p *Engine) Run() {
	// Signal errors from the read/write goroutines. Don't block if we don't
	// read all the errors, we only care about the first.
	error := make(chan error, 2)
	wait := sync.WaitGroup{}
	wait.Add(2)

	go func() { // Read goroutine
		defer wait.Done()
		for {
			rbuf := p.read.buffer()
			n, err := p.conn.Read(rbuf)
			if n > 0 {
				p.read.buffers <- rbuf[:n]
			} else if err != nil {
				close(p.read.buffers)
				error <- err
				return
			}
		}
	}()

	go func() { // Write goroutine
		defer wait.Done()
		for {
			wbuf, ok := <-p.write.buffers
			if !ok {
				return
			}
			_, err := p.conn.Write(wbuf)
			if err != nil {
				error <- err
				return
			}
		}
	}()

	wbuf := p.write.buffer()[:0]
loop:
	for {
		if len(wbuf) == 0 {
			p.pop(&wbuf)
		}
		// Don't set wchan unless there is something to write.
		var wchan chan []byte
		if len(wbuf) > 0 {
			wchan = p.write.buffers
		}

		select {
		case buf := <-p.read.buffers: // Read a buffer
			p.push(buf)
		case wchan <- wbuf: // Write a buffer
			wbuf = p.write.buffer()[:0]
		case f := <-p.inject: // Function injected from another goroutine
			f()
		case err := <-error: // Network read or write error
			p.conn.Close() // Make sure both sides are closed
			p.err.Set(err)
			p.transport.CloseHead()
			p.transport.CloseTail()
		}
		p.process()
		if p.err.Get() != nil {
			break loop
		}
	}
	close(p.write.buffers)
	p.conn.Close()
	wait.Wait()
	connectionContexts.Delete(p.connection)
	if !p.connection.IsNil() {
		p.connection.Free()
	}
	if !p.transport.IsNil() {
		p.transport.Free()
	}
	if p.collector != nil {
		C.pn_collector_free(p.collector)
	}
	for _, h := range p.handlers {
		switch h := h.(type) {
		case cHandler:
			C.pn_handler_free(h.pn)
		}
	}
	close(p.running) // Signal goroutines have exited and Error is set.
}

func minInt(a, b int) int {
	if a < b {
		return a
	} else {
		return b
	}
}

func (p *Engine) pop(buf *[]byte) {
	pending := int(p.transport.Pending())
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
	C.memcpy(unsafe.Pointer(&(*buf)[0]), p.transport.Head(), C.size_t(size))
	internal.Assert(size > 0)
	p.transport.Pop(uint(size))
}

func (p *Engine) push(buf []byte) {
	buf2 := buf
	for len(buf2) > 0 {
		n := p.transport.Push(buf2)
		if n <= 0 {
			panic(internal.Errorf("error in transport: %s", internal.PnErrorCode(n)))
		}
		buf2 = buf2[n:]
	}
}

func (p *Engine) handle(e Event) (more bool) {
	for _, h := range p.handlers {
		h.HandleEvent(e)
	}
	if e.Type() == ETransportClosed {
		p.err.Set(e.Connection().RemoteCondition().Error())
		p.err.Set(e.Connection().Transport().Condition().Error())
		if p.err.Get() == nil {
			p.err.Set(io.EOF)
		}
		return false
	}
	return true
}

func (p *Engine) process() (more bool) {
	for ce := C.pn_collector_peek(p.collector); ce != nil; ce = C.pn_collector_peek(p.collector) {
		e := makeEvent(ce)
		if !p.handle(e) {
			return false
		}
		C.pn_collector_pop(p.collector)
	}
	return true
}

func (p *Engine) Connection() Connection { return p.connection }
