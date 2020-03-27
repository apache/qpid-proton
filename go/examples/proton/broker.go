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

// TODO: show how to handle acknowledgements from receivers and put rejected or
// un-acknowledged messages back on their queues.

package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/proton"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `
Usage: %s
A simple broker-like demo. Queues are created automatically for sender or receiver addresses.
`, os.Args[0])
	flag.PrintDefaults()
}

var addr = flag.String("addr", ":amqp", "Listening address")
var credit = flag.Int("credit", 100, "Receiver credit window")
var qsize = flag.Int("qsize", 1000, "Max queue size")
var debug = flag.Bool("debug", false, "Print detailed debug output")
var debugf = func(format string, data ...interface{}) {} // Default no debugging output

func main() {
	flag.Usage = usage
	flag.Parse()
	if *debug {
		debugf = func(format string, data ...interface{}) { log.Printf(format, data...) }
	}
	b := &broker{makeQueues(*qsize)}
	if err := b.run(); err != nil {
		log.Fatal(err)
	}
}

// State for the broker
type broker struct {
	queues queues
}

// Listens for connections and starts a proton.Engine for each one.
func (b *broker) run() error {
	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		return err
	}
	defer listener.Close()
	fmt.Printf("Listening on %s\n", listener.Addr())
	for {
		conn, err := listener.Accept()
		if err != nil {
			debugf("Accept error: %v", err)
			continue
		}
		adapter := proton.NewMessagingAdapter(newHandler(&b.queues))
		// We want to accept messages when they are enqueued, not just when they
		// are received, so we turn off auto-accept and prefetch by the adapter.
		adapter.Prefetch = 0
		adapter.AutoAccept = false
		engine, err := proton.NewEngine(conn, adapter)
		if err != nil {
			debugf("Connection error: %v", err)
			continue
		}
		engine.Server() // Enable server-side protocol negotiation.
		debugf("Accepted connection %s", engine)
		go func() { // Start goroutine to run the engine event loop
			engine.Run()
			debugf("Closed %s", engine)
		}()
	}
}

// handler handles AMQP events. There is one handler per connection.  The
// handler does not need to be concurrent-safe as proton.Engine will serialize
// all calls to the handler. We use channels to communicate between the handler
// goroutine and other goroutines sending and receiving messages.
type handler struct {
	queues    *queues
	receivers map[proton.Link]*receiver
	senders   map[proton.Link]*sender
	injecter  proton.Injecter
}

func newHandler(queues *queues) *handler {
	return &handler{
		queues:    queues,
		receivers: make(map[proton.Link]*receiver),
		senders:   make(map[proton.Link]*sender),
	}
}

// HandleMessagingEvent handles an event, called in the handler goroutine.
func (h *handler) HandleMessagingEvent(t proton.MessagingEvent, e proton.Event) {
	switch t {

	case proton.MStart:
		h.injecter = e.Injecter()

	case proton.MLinkOpening:
		if e.Link().IsReceiver() {
			h.startReceiver(e)
		} else {
			h.startSender(e)
		}

	case proton.MLinkClosed:
		h.linkClosed(e.Link(), e.Link().RemoteCondition().Error())

	case proton.MSendable:
		if s, ok := h.senders[e.Link()]; ok {
			s.sendable() // Signal the send goroutine that we have credit.
		} else {
			proton.CloseError(e.Link(), amqp.Errorf(amqp.NotFound, "link %s sender not found", e.Link()))
		}

	case proton.MMessage:
		m, err := e.Delivery().Message() // Message() must be called while handling the MMessage event.
		if err != nil {
			proton.CloseError(e.Link(), err)
			break
		}
		r, ok := h.receivers[e.Link()]
		if !ok {
			proton.CloseError(e.Link(), amqp.Errorf(amqp.NotFound, "link %s receiver not found", e.Link()))
			break
		}
		// This will not block as AMQP credit is set to the buffer capacity.
		r.buffer <- receivedMessage{e.Delivery(), m}
		debugf("link %s received %#v", e.Link(), m)

	case proton.MConnectionClosed, proton.MDisconnected:
		for l, _ := range h.receivers {
			h.linkClosed(l, nil)
		}
		for l, _ := range h.senders {
			h.linkClosed(l, nil)
		}
	}
}

// linkClosed is called when a link has been closed by both ends.
// It removes the link from the handlers maps and stops its goroutine.
func (h *handler) linkClosed(l proton.Link, err error) {
	if s, ok := h.senders[l]; ok {
		s.stop()
		delete(h.senders, l)
	} else if r, ok := h.receivers[l]; ok {
		r.stop()
		delete(h.receivers, l)
	}
}

// link has some common data and methods that are used by the sender and receiver types.
//
// An active link is represented by a sender or receiver value and a goroutine
// running its run() method. The run() method communicates with the handler via
// channels.
type link struct {
	l proton.Link
	q queue
	h *handler
}

func makeLink(l proton.Link, q queue, h *handler) link {
	lnk := link{l: l, q: q, h: h}
	return lnk
}

// receiver has a channel to buffer messages that have been received by the
// handler and are waiting to go on the queue. AMQP credit ensures that the
// handler does not overflow the buffer and block.
type receiver struct {
	link
	buffer chan receivedMessage
}

// receivedMessage holds a message and a Delivery so that the message can be
// acknowledged when it is put on the queue.
type receivedMessage struct {
	delivery proton.Delivery
	message  amqp.Message
}

// startReceiver creates a receiver and a goroutine for its run() method.
func (h *handler) startReceiver(e proton.Event) {
	q := h.queues.Get(e.Link().RemoteTarget().Address())
	r := &receiver{
		link:   makeLink(e.Link(), q, h),
		buffer: make(chan receivedMessage, *credit),
	}
	h.receivers[r.l] = r
	r.l.Flow(cap(r.buffer)) // Give credit to fill the buffer to capacity.
	go r.run()
}

// run runs in a separate goroutine. It moves messages from the buffer to the
// queue for a receiver link, and injects a handler function to acknowledge the
// message and send a credit.
func (r *receiver) run() {
	for rm := range r.buffer {
		r.q <- rm.message
		d := rm.delivery
		// We are not in the handler goroutine so we Inject the accept function as a closure.
		r.h.injecter.Inject(func() {
			// Check that the receiver is still open, it may have been closed by the remote end.
			if r == r.h.receivers[r.l] {
				d.Accept()  // Accept the delivery
				r.l.Flow(1) // Add one credit
			}
		})
	}
}

// stop closes the buffer channel and waits for the run() goroutine to stop.
func (r *receiver) stop() {
	close(r.buffer)
}

// sender has a channel that is used to signal when there is credit to send messages.
type sender struct {
	link
	credit chan struct{} // Channel to signal availability of credit.
}

// startSender creates a sender and starts a goroutine for sender.run()
func (h *handler) startSender(e proton.Event) {
	q := h.queues.Get(e.Link().RemoteSource().Address())
	s := &sender{
		link:   makeLink(e.Link(), q, h),
		credit: make(chan struct{}, 1), // Capacity of 1 for signalling.
	}
	h.senders[e.Link()] = s
	go s.run()
}

// stop closes the credit channel and waits for the run() goroutine to stop.
func (s *sender) stop() {
	close(s.credit)
}

// sendable signals that the sender has credit, it does not block.
// sender.credit has capacity 1, if it is already full we carry on.
func (s *sender) sendable() {
	select { // Non-blocking
	case s.credit <- struct{}{}:
	default:
	}
}

// run runs in a separate goroutine. It monitors the queue for messages and injects
// a function to send them when there is credit
func (s *sender) run() {
	var q queue // q is nil initially as we have no credit.
	for {
		select {
		case _, ok := <-s.credit:
			if !ok { // sender closed
				return
			}
			q = s.q // We have credit, enable selecting on the queue.

		case m, ok := <-q: // q is only enabled when we have credit.
			if !ok { // queue closed
				return
			}
			q = nil                      // Assume all credit will be used used, will be signaled otherwise.
			s.h.injecter.Inject(func() { // Inject handler function to actually send
				if s.h.senders[s.l] != s { // The sender has been closed by the remote end.
					q.PutBack(m) // Put the message back on the queue but don't block
					return
				}
				if s.sendOne(m) != nil {
					return
				}
				// Send as many more messages as we can without blocking
				for s.l.Credit() > 0 {
					select { // Non blocking receive from q
					case m, ok := <-s.q:
						if ok {
							s.sendOne(m)
						}
					default: // Queue is empty but we have credit, signal the run() goroutine.
						s.sendable()
					}
				}
			})
		}
	}
}

// sendOne runs in the handler goroutine. It sends a single message.
func (s *sender) sendOne(m amqp.Message) error {
	delivery, err := s.l.Send(m)
	if err == nil {
		delivery.Settle() // Pre-settled, unreliable.
		debugf("link %s sent %#v", s.l, m)
	} else {
		s.q.PutBack(m) // Put the message back on the queue, don't block
	}
	return err
}

// Use a buffered channel as a very simple queue.
type queue chan amqp.Message

// Put a message back on the queue, does not block.
func (q queue) PutBack(m amqp.Message) {
	select {
	case q <- m:
	default:
		// Not an efficient implementation but ensures we don't block the caller.
		go func() { q <- m }()
	}
}

// Concurrent-safe map of queues.
type queues struct {
	queueSize int
	m         map[string]queue
	lock      sync.Mutex
}

func makeQueues(queueSize int) queues {
	return queues{queueSize: queueSize, m: make(map[string]queue)}
}

// Create a queue if not found.
func (qs *queues) Get(name string) queue {
	qs.lock.Lock()
	defer qs.lock.Unlock()
	q := qs.m[name]
	if q == nil {
		q = make(queue, qs.queueSize)
		qs.m[name] = q
	}
	return q
}
