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

package event

// #include <proton/connection.h>
// #include <proton/transport.h>
// #include <proton/event.h>
// #include <proton/reactor.h>
// #include <proton/handlers.h>
// #include <proton/transport.h>
// #include <proton/session.h>
// #include <memory.h>
// #include <stdlib.h>
//
// PN_HANDLE(REMOTE_ADDR)
import "C"

import (
	"fmt"
	"io"
	"net"
	"qpid.apache.org/proton/go/internal"
	"sync"
	"unsafe"
)

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

// FIXME aconway 2015-05-04: direct sending to Inject may block user goroutines if
// the pum stops. Make this a function that selects on running.

// FIXME aconway 2015-05-05: for consistency should Pump be called Driver?

/*
Pump reads from a net.Conn, decodes AMQP events and calls the appropriate
Handler functions. Actions taken by Handler functions (such as sending messages)
are encoded and written to the net.Conn.

The proton protocol engine is single threaded (per connection). The Pump runs
proton in the goroutine that calls Pump.Run() and creates goroutines to feed
data to/from a net.Conn. You can create multiple Pumps to handle multiple
connections concurrently.

Methods in this package can only be called in the goroutine that executes the
corresponding Pump.Run(). You implement the EventHandler or MessagingHandler
interfaces and provide those values to NewPump(). Their HandleEvent method will be
called in the Pump goroutine, in typical event-driven style.

Handlers can pass values from an event (Connections, Links, Deliveries etc.) to
other goroutines, store them, or use them as map indexes. Effectively they are
just C pointers.  Other goroutines cannot call their methods directly but they
can can create function closures that call their methods and send those closures
to the Pump.Inject channel. They will execute safely in the pump
goroutine. Injected functions, or your handlers, can set up channels to get
results back to other goroutines.

You are responsible for ensuring you don't use an event value after the C object
has been deleted. The handler methods will tell you when a value is no longer
valid. For example after a MethodHandler handles a LinkClosed event, that link
is no longer valid. If you do Link.Close() yourself (in a handler or injected
function) the link remains valid until the corresponing LinkClosed event is
received by the handler.

Pump.Close() will take care of cleaning up any remaining values and types when
you are done with the Pump. All values associated with a pump become invalid
when you call Pump.Close()

The qpid.apache.org/proton/go/messaging package will do all this for you, so unless
you are doing something fairly low-level it is probably a better choice.

*/
type Pump struct {
	// Error is set on exit from Run() if there was an error.
	Error error
	// Channel to inject functions to be executed in the Pump's proton event loop.
	Inject chan func()

	conn       net.Conn
	transport  *C.pn_transport_t
	connection *C.pn_connection_t
	collector  *C.pn_collector_t
	read       *bufferChan    // Read buffers channel.
	write      *bufferChan    // Write buffers channel.
	handlers   []EventHandler // Handlers for proton events.
	running    chan struct{}  // This channel will be closed when the goroutines are done.
}

const bufferSize = 4096

var pumps map[*C.pn_connection_t]*Pump

func init() {
	pumps = make(map[*C.pn_connection_t]*Pump)
}

// NewPump initializes a pump with a connection and handlers. To start it running:
//    p := NewPump(...)
//    go run p.Run()
// The goroutine will exit when the pump is closed or disconnected.
// You can check for errors on Pump.Error.
//
func NewPump(conn net.Conn, handlers ...EventHandler) (*Pump, error) {
	// Save the connection ID for Connection.String()
	p := &Pump{
		Inject:     make(chan func(), 100), // FIXME aconway 2015-05-04: blocking hack
		conn:       conn,
		transport:  C.pn_transport(),
		connection: C.pn_connection(),
		collector:  C.pn_collector(),
		handlers:   handlers,
		read:       newBufferChan(bufferSize),
		write:      newBufferChan(bufferSize),
		running:    make(chan struct{}),
	}
	if p.transport == nil || p.connection == nil || p.collector == nil {
		return nil, internal.Errorf("failed to allocate pump")
	}
	pnErr := int(C.pn_transport_bind(p.transport, p.connection))
	if pnErr != 0 {
		return nil, internal.Errorf("cannot setup pump: %s", internal.PnErrorCode(pnErr))
	}
	C.pn_connection_collect(p.connection, p.collector)
	C.pn_connection_open(p.connection)
	pumps[p.connection] = p
	return p, nil
}

func (p *Pump) String() string {
	return fmt.Sprintf("(%s-%s)", p.conn.LocalAddr(), p.conn.RemoteAddr())
}

func (p *Pump) Id() string {
	return fmt.Sprintf("%p", &p)
}

// setError sets error only if not already set
func (p *Pump) setError(e error) {
	if p.Error == nil {
		p.Error = e
	}
}

// Server puts the Pump in server mode, meaning it will auto-detect security settings on
// the incoming connnection such as use of SASL and SSL.
// Must be called before Run()
//
func (p *Pump) Server() {
	C.pn_transport_set_server(p.transport)
}

func (p *Pump) free() {
	if p.connection != nil {
		C.pn_connection_free(p.connection)
	}
	if p.transport != nil {
		C.pn_transport_free(p.transport)
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
}

// Close closes the AMQP connection, the net.Conn, and stops associated goroutines.
// It will cause Run() to return. Run() may return earlier if the network disconnects
// but you must still call Close() to clean everything up.
//
// Methods on values associated with the pump (Connections, Sessions, Links) will panic
// if called after Close()
//
func (p *Pump) Close() error {
	// If the pump is still running, inject a close. Either way wait for it to finish.
	select {
	case p.Inject <- func() { C.pn_connection_close(p.connection) }:
		<-p.running // Wait to finish
	case <-p.running: // Wait for goroutines to finish
	}
	delete(pumps, p.connection)
	p.free()
	return p.Error
}

// Run the pump. Normally called in a goroutine as: go pump.Run()
// An error dunring Run is stored on p.Error.
//
func (p *Pump) Run() {
	// Signal errors from the read/write goroutines. Don't block if we don't
	// read all the errors, we only care about the first.
	error := make(chan error, 2)
	// FIXME aconway 2015-05-04: 	stop := make(chan struct{}) // Closed to signal that read/write should stop.

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
		case f := <-p.Inject: // Function injected from another goroutine
			f()
		case err := <-error: // Read or write error
			p.setError(err)
			C.pn_transport_close_tail(p.transport)
			C.pn_transport_close_head(p.transport)
		}
		if err := p.process(); err != nil {
			p.setError(err)
			break loop
		}
	}
	close(p.write.buffers)
	p.conn.Close()
	wait.Wait()
	close(p.running) // Signal goroutines have exited and Error is set.
}

func minInt(a, b int) int {
	if a < b {
		return a
	} else {
		return b
	}
}

func (p *Pump) pop(buf *[]byte) {
	pending := int(C.pn_transport_pending(p.transport))
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
	C.memcpy(unsafe.Pointer(&(*buf)[0]), unsafe.Pointer(C.pn_transport_head(p.transport)), C.size_t(size))
	C.pn_transport_pop(p.transport, C.size_t(size))
}

func (p *Pump) push(buf []byte) {
	buf2 := buf
	for len(buf2) > 0 {
		n := int(C.pn_transport_push(p.transport, (*C.char)(unsafe.Pointer((&buf2[0]))), C.size_t(len(buf2))))
		if n <= 0 {
			panic(internal.Errorf("error in transport: %s", internal.PnErrorCode(n)))
		}
		buf2 = buf2[n:]
	}
}

func (p *Pump) handle(e Event) error {
	for _, h := range p.handlers {
		if err := h.HandleEvent(e); err != nil {
			return err
		}
	}
	if e.Type() == ETransportClosed {
		return io.EOF
	}
	return nil
}

func (p *Pump) process() error {
	// FIXME aconway 2015-05-04: if a Handler returns error we should stop the pump
	for ce := C.pn_collector_peek(p.collector); ce != nil; ce = C.pn_collector_peek(p.collector) {
		e := makeEvent(ce)
		if err := p.handle(e); err != nil {
			return err
		}
		C.pn_collector_pop(p.collector)
	}
	return nil
}

// Connectoin gets the Pump's connection value.
func (p *Pump) Connection() Connection { return Connection{p.connection} }
