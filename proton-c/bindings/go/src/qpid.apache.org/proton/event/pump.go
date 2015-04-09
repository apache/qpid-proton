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
// #include <memory.h>
//
import "C"

import (
	"net"
	"sync"
	"unsafe"
)

// FIXME aconway 2015-04-09: We should allow data from multiple connections to be pumped
// into a single event loop (using the proton Reactor)
// That would allow the user to decide if they want an event-loop goroutine per connection
// or if they want to handle several connections in one event loop.

// Pump reads from a net.Conn, decodes AMQP events and calls the appropriate
// Handler functions. Actions taken by Handler functions (such as sending messages)
// are encoded and written to the net.Conn.
//
type Pump struct {
	conn       net.Conn
	transport  *C.pn_transport_t
	connection *C.pn_connection_t
	collector  *C.pn_collector_t
	read       chan []byte    // Read buffers channel. Will close when pump closes.
	write      chan []byte    // Write buffer channel. Must be closed when read closes.
	waiter     sync.WaitGroup // Wait for read and write goroutines to complete.
	handlers   []CoreHandler  // Handlers for proton events.

	inject chan func()   // Functions inject into the loop
	closed chan struct{} // This channel will be closed when the remote end closes.
}

const bufferSize = 4096

// NewPump initializes a pump with a connection and handlers.. Call `go Run()` to start it running.
func NewPump(conn net.Conn, handlers ...CoreHandler) (*Pump, error) {
	p := &Pump{
		conn:       conn,
		transport:  C.pn_transport(),
		connection: C.pn_connection(),
		collector:  C.pn_collector(),
		handlers:   handlers,
		read:       make(chan []byte),
		write:      make(chan []byte),
		inject:     make(chan func()),
		closed:     make(chan struct{}),
	}
	p.handlers = append(p.handlers, handlers...)

	if p.transport == nil || p.connection == nil || p.collector == nil {
		return nil, errorf("failed to allocate pump")
	}
	pnErr := int(C.pn_transport_bind(p.transport, p.connection))
	if pnErr != 0 {
		return nil, errorf("cannot setup pump: %s", pnErrorName(pnErr))
	}
	C.pn_connection_collect(p.connection, p.collector)
	C.pn_connection_open(p.connection)
	return p, nil
}

// Server puts the Pump in server mode, meaning it will auto-detect security settings on
// the incoming connnection such as use of SASL and SSL.
// Must be called before Run()
func (p *Pump) Server() {
	C.pn_transport_set_server(p.transport)
}

// Run the pump. Normally called in a goroutine as: go pump.Run()
func (p *Pump) Run() {
	go p.run()
}

// Pump handles the connction close event to close itself.
func (p *Pump) Handle(e Event) error {
	switch e.Type() {
	case EConnectionLocalClose:
		return p.close()
	}
	return nil
}

// Closing the pump will also close the net.Conn and stop associated goroutines.
func (p *Pump) Close() error {
	// FIXME aconway 2015-04-08: broken

	// Note this is called externally, outside the proton event loop.
	// Polite AMQP close
	p.inject <- func() { C.pn_connection_close(p.connection) }
	_, _ = <-p.closed // Wait for remote close
	return p.close()
}

// close private implementation, call in the event loop.
func (p *Pump) close() error {
	p.conn.Close()
	p.waiter.Wait()
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
	return nil // FIXME aconway 2015-03-31: error handling
}

// Start goroutines to feed the pn_transport_t from the pump.
func (c *Pump) run() error {
	// FIXME aconway 2015-03-17: error handling
	c.waiter.Add(2)
	var readError, writeError error

	go func() { // Read
		rbuf, rbuf2 := make([]byte, bufferSize), make([]byte, bufferSize)
		for {
			rbuf = rbuf[:cap(rbuf)]
			n, err := c.conn.Read(rbuf)
			if n > 0 {
				c.read <- rbuf[:n]
			}
			if err != nil {
				readError = err
				break
			}
			rbuf, rbuf2 = rbuf2, rbuf // Swap the buffers, fill the one not in use.
		}
		close(c.read)
		c.waiter.Done()
	}()

	go func() { // Write
		for wbuf := range c.write {
			_, err := c.conn.Write(wbuf)
			if err != nil {
				writeError = err
				break
			}
		}
		c.waiter.Done()
	}()

	// Proton driver loop
	wbuf, wbuf2 := make([]byte, bufferSize), make([]byte, bufferSize)
	wbuf = c.pop(wbuf) // First write buffer
	for {              // handle pn_transport_t
		select {
		case buf, ok := <-c.read: // Read a buffer
			if !ok { // Read channel closed
				break
			}
			c.push(buf)

		case c.write <- wbuf: // Write a buffer
			wbuf, wbuf2 = wbuf2, wbuf // Swap the buffers, fill the unused one.
			wbuf = c.pop(wbuf)        // Next buffer to write

		case f := <-c.inject: // Function injected from another goroutine
			f()
		}
		c.process() // FIXME aconway 2015-03-17: error handling
	}

	close(c.write)
	c.waiter.Wait() // Wait for read/write goroutines to finish
	switch {
	case readError != nil:
		return readError
	case writeError != nil:
		return writeError
	}
	return nil
}

func minInt(a, b int) int {
	if a < b {
		return a
	} else {
		return b
	}
}

func (c *Pump) pop(buf []byte) []byte {
	pending := int(C.pn_transport_pending(c.transport))
	if pending == int(C.PN_EOS) {
		return nil
	}
	if pending < 0 {
		panic(errorf(pnErrorName(pending)))
	}
	size := minInt(pending, cap(buf))
	buf = buf[:size]
	if size == 0 {
		return buf
	}
	C.memcpy(unsafe.Pointer(&buf[0]), unsafe.Pointer(C.pn_transport_head(c.transport)), C.size_t(size))
	C.pn_transport_pop(c.transport, C.size_t(size))
	return buf
}

func (c *Pump) push(buf []byte) {
	buf2 := buf
	for len(buf2) > 0 {
		n := int(C.pn_transport_push(c.transport, (*C.char)(unsafe.Pointer((&buf2[0]))), C.size_t(len(buf2))))
		if n <= 0 {
			panic(errorf("error in transport: %s", pnErrorName(n)))
		}
		buf2 = buf2[n:]
	}
}

func (c *Pump) process() {
	for ce := C.pn_collector_peek(c.collector); ce != nil; ce = C.pn_collector_peek(c.collector) {
		e := Event{ce}
		for _, h := range c.handlers {
			h.Handle(e) // FIXME aconway 2015-03-18: error handling
		}
		C.pn_collector_pop(c.collector)
	}
}

func (c *Pump) AddHandlers(handlers ...CoreHandler) {
	c.inject <- func() {
		c.handlers = append(c.handlers, handlers...)
	}
}
