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

//
// This is a simple AMQP broker implemented using the event-driven proton package.
//
// It maintains a set of named in-memory queues of messages. Clients can send
// messages to queues or subscribe to receive messages from them.
//

package main

import (
	"./util"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"qpid.apache.org/amqp"
	"qpid.apache.org/proton"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `
Usage: %s
A simple broker-like demo. Queues are created automatically for sender or receiver addrsses.
`, os.Args[0])
	flag.PrintDefaults()
}

var addr = flag.String("addr", ":amqp", "Listening address")
var credit = flag.Int("credit", 100, "Receiver credit window")
var qsize = flag.Int("qsize", 1000, "Max queue size")

func main() {
	flag.Usage = usage
	flag.Parse()

	b := newBroker()
	listener, err := net.Listen("tcp", *addr)
	util.ExitIf(err)
	defer listener.Close()
	fmt.Printf("Listening on %s\n", listener.Addr())

	// Loop accepting new connections.
	for {
		conn, err := listener.Accept()
		if err != nil {
			util.Debugf("Accept error: %s", err)
			continue
		}
		if err := b.connection(conn); err != nil {
			if err != nil {
				util.Debugf("Connection error: %s", err)
				continue
			}
		}
	}
}

type broker struct {
	queues util.Queues
}

func newBroker() *broker {
	return &broker{util.MakeQueues(*qsize)}
}

// connection creates a new AMQP connection for a net.Conn.
func (b *broker) connection(conn net.Conn) error {
	delegator := proton.NewMessagingDelegator(newHandler(&b.queues, *credit))
	// We want to accept messages when they are enqueued, not just when they
	// are received, so we turn off auto-accept and prefetch by the handler.
	delegator.Prefetch = 0
	delegator.AutoAccept = false
	engine, err := proton.NewEngine(conn, delegator)
	if err != nil {
		return err
	}
	engine.Server() // Enable server-side protocol negotiation.
	go func() {     // Start goroutine to run the engine event loop
		engine.Run()
		util.Debugf("Closed %s", engine)
	}()
	util.Debugf("Accepted %s", engine)
	return nil
}

// receiver is a channel to buffer messages waiting to go on the queue.
type receiver chan receivedMessage

// receivedMessage is a message and the corresponding delivery for acknowledgement.
type receivedMessage struct {
	delivery proton.Delivery
	message  amqp.Message
}

// sender is a signal channel, closed when we are done sending.
type sender chan struct{}

// handler handles AMQP events. There is one handler per connection.  The
// handler does not need to be concurrent-safe as proton will serialize all
// calls to a handler. We will use channels to communicate from the handler
// to goroutines sending and receiving messages.
type handler struct {
	queues    *util.Queues
	credit    int // Credit window for receiver flow control.
	receivers map[proton.Link]receiver
	senders   map[proton.Link]sender
}

func newHandler(queues *util.Queues, credit int) *handler {
	return &handler{
		queues,
		credit,
		make(map[proton.Link]receiver),
		make(map[proton.Link]sender),
	}
}

// Handle an AMQP event.
func (h *handler) HandleMessagingEvent(t proton.MessagingEvent, e proton.Event) {
	switch t {

	case proton.MLinkOpening:
		l := e.Link()
		var err error
		if l.IsReceiver() {
			err = h.receiver(l)
		} else { // IsSender()
			err = h.sender(l)
		}
		if err == nil {
			util.Debugf("%s opened", l)
		} else {
			util.Debugf("%s open error: %s", l, err)
			proton.CloseError(l, err)
		}

	case proton.MLinkClosing:
		l := e.Link()
		if r, ok := h.receivers[l]; ok {
			close(r)
			delete(h.receivers, l)
		} else if s, ok := h.senders[l]; ok {
			close(s)
			delete(h.senders, l)
		}
		util.Debugf("%s closed", l)

	case proton.MSendable:
		l := e.Link()
		q := h.queues.Get(l.RemoteSource().Address())
		if n, err := h.sendAll(e.Link(), q); err == nil && n > 0 {
			// Still have credit, start a watcher.
			go h.sendWatch(e.Link(), q)
		}

	case proton.MMessage:
		l := e.Link()
		d := e.Delivery()
		m, err := d.Message() // Must decode message immediately before link state changes.
		if err != nil {
			util.Debugf("%s error decoding message: %s", e.Link(), err)
			proton.CloseError(l, err)
		} else {
			// This will not block, AMQP credit prevents us from overflowing the buffer.
			h.receivers[l] <- receivedMessage{d, m}
			util.Debugf("%s received %s", l, util.FormatMessage(m))
		}

	case proton.MConnectionClosing, proton.MDisconnected:
		for l, r := range h.receivers {
			close(r)
			delete(h.receivers, l)
		}
		for l, s := range h.senders {
			close(s)
			delete(h.senders, l)
		}
	}
}

// receiver is called by the handler when a receiver link opens.
//
// It sets up data structures in the handler and then starts a goroutine
// to receive messages and put them on a queue.
func (h *handler) receiver(l proton.Link) error {
	q := h.queues.Get(l.RemoteTarget().Address())
	buffer := make(receiver, h.credit)
	h.receivers[l] = buffer
	l.Flow(cap(buffer)) // credit==cap(buffer) so we won't overflow the buffer.
	go h.runReceive(l, buffer, q)
	return nil
}

// runReceive moves messages from buffer to queue
func (h *handler) runReceive(l proton.Link, buffer receiver, q util.Queue) {
	for rm := range buffer {
		q <- rm.message
		rm2 := rm // Save in temp var for injected closure
		err := l.Connection().Injecter().Inject(func() {
			rm2.delivery.Accept()
			l.Flow(1)
		})
		if err != nil {
			util.Debugf("%s receive error: %s", l, err)
			proton.CloseError(l, err)
		}
	}
}

// sender is called by the handler when a sender link opens.
// It sets up a sender structures in the handler.
func (h *handler) sender(l proton.Link) error {
	h.senders[l] = make(sender)
	return nil
}

// send one message in handler context, assumes we have credit.
func (h *handler) send(l proton.Link, m amqp.Message, q util.Queue) error {
	delivery, err := l.Send(m)
	if err != nil {
		h.closeSender(l, err)
		return err
	}
	delivery.Settle() // Pre-settled, unreliable.
	util.Debugf("%s sent %s", l, util.FormatMessage(m))
	return nil
}

// sendAll sends as many messages as possible without blocking, call in handler context.
// Returns the number of credits left, >0 means we ran out of messages.
func (h *handler) sendAll(l proton.Link, q util.Queue) (int, error) {
	for l.Credit() > 0 {
		select {
		case m, ok := <-q:
			if ok { // Got a message
				if err := h.send(l, m, q); err != nil {
					return 0, err
				}
			} else { // Queue is closed
				l.Close()
				return 0, io.EOF
			}
		default: // Queue empty
			return l.Credit(), nil
		}
	}
	return l.Credit(), nil
}

// sendWatch watches the queue for more messages and re-runs sendAll.
// Run in a separate goroutine, so must inject handler functions.
func (h *handler) sendWatch(l proton.Link, q util.Queue) {
	select {
	case m, ok := <-q:
		l.Connection().Injecter().Inject(func() {
			if ok {
				if h.send(l, m, q) != nil {
					return
				}
				if n, err := h.sendAll(l, q); err != nil {
					return
				} else if n > 0 {
					go h.sendWatch(l, q) // Start a new watcher.
				}
			}
		})
	case <-h.senders[l]: // Closed
		return
	}
}

// closeSender closes a sender link and signals goroutines processing that sender.
func (h *handler) closeSender(l proton.Link, err error) {
	util.Debugf("%s sender closed: %s", l, err)
	proton.CloseError(l, err)
	close(h.senders[l])
	delete(h.senders, l)
}
