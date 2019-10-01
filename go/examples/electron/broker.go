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
// This is a simple AMQP broker implemented using the procedural electron package.
//
// It maintains a set of named in-memory queues of messages. Clients can send
// messages to queues or subscribe to receive messages from them.
//

package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/electron"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `
Usage: %s
A simple message broker.
Queues are created automatically for sender or receiver addresses.
`, os.Args[0])
	flag.PrintDefaults()
}

var addr = flag.String("addr", ":amqp", "Network address to listen on, in the form \"host:port\"")
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

	b := &broker{
		queues:    makeQueues(*qsize),
		container: electron.NewContainer(fmt.Sprintf("broker[%v]", os.Getpid())),
		acks:      make(chan electron.Outcome),
		sent:      make(chan sentMessage),
	}
	if err := b.run(); err != nil {
		log.Fatal(err)
	}
}

// State for the broker
type broker struct {
	queues    queues                // A collection of queues.
	container electron.Container    // electron.Container manages AMQP connections.
	sent      chan sentMessage      // Channel to record sent messages.
	acks      chan electron.Outcome // Channel to receive the Outcome of sent messages.
}

// Record of a sent message and the queue it came from.
// If a message is rejected or not acknowledged due to a failure, we will put it back on the queue.
type sentMessage struct {
	m amqp.Message
	q queue
}

// run listens for incoming net.Conn connections and starts an electron.Connection for each one.
func (b *broker) run() error {
	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		return err
	}
	defer listener.Close()
	fmt.Printf("Listening on %v\n", listener.Addr())

	go b.acknowledgements() // Handles acknowledgements for all connections.

	// Start a goroutine for each new connections
	for {
		c, err := b.container.Accept(listener)
		if err != nil {
			debugf("Accept error: %v", err)
			continue
		}
		cc := &connection{b, c}
		go cc.run() // Handle the connection
		debugf("Accepted %v", c)
	}
}

// State for a broker connection
type connection struct {
	broker     *broker
	connection electron.Connection
}

// accept remotely-opened endpoints (Session, Sender and Receiver) on a connection
// and start goroutines to service them.
func (c *connection) run() {
	for in := range c.connection.Incoming() {
		debugf("incoming %v", in)

		switch in := in.(type) {

		case *electron.IncomingSender:
			s := in.Accept().(electron.Sender)
			go c.sender(s)

		case *electron.IncomingReceiver:
			in.SetPrefetch(true)
			in.SetCapacity(*credit) // Pre-fetch up to credit window.
			r := in.Accept().(electron.Receiver)
			go c.receiver(r)

		default:
			in.Accept() // Accept sessions unconditionally
		}
	}
	debugf("incoming closed: %v", c.connection)
}

// receiver receives messages and pushes to a queue.
func (c *connection) receiver(receiver electron.Receiver) {
	q := c.broker.queues.Get(receiver.Target())
	for {
		if rm, err := receiver.Receive(); err == nil {
			debugf("%v: received %v", receiver, rm.Message.Body())
			q <- rm.Message
			rm.Accept()
		} else {
			debugf("%v error: %v", receiver, err)
			break
		}
	}
}

// sender pops messages from a queue and sends them.
func (c *connection) sender(sender electron.Sender) {
	q := c.broker.queues.Get(sender.Source())
	for {
		if sender.Error() != nil {
			debugf("%v closed: %v", sender, sender.Error())
			return
		}
		select {

		case m := <-q:
			debugf("%v: sent %v", sender, m.Body())
			sm := sentMessage{m, q}
			c.broker.sent <- sm                    // Record sent message
			sender.SendAsync(m, c.broker.acks, sm) // Receive outcome on c.broker.acks with Value sm

		case <-sender.Done(): // break if sender is closed
			break
		}
	}
}

// acknowledgements keeps track of sent messages and receives outcomes.
//
// We could have handled outcomes separately per-connection, per-sender or even
// per-message. Message outcomes are returned via channels defined by the user
// so they can be grouped in any way that suits the application.
func (b *broker) acknowledgements() {
	sentMap := make(map[sentMessage]bool)
	for {
		select {
		case sm, ok := <-b.sent: // A local sender records that it has sent a message.
			if ok {
				sentMap[sm] = true
			} else {
				return // Closed
			}
		case outcome := <-b.acks: // The message outcome is available
			sm := outcome.Value.(sentMessage)
			delete(sentMap, sm)
			if outcome.Status != electron.Accepted { // Error, release or rejection
				sm.q.PutBack(sm.m) // Put the message back on the queue.
				debugf("message %v put back, status %v, error %v", sm.m.Body(), outcome.Status, outcome.Error)
			}
		}
	}
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
