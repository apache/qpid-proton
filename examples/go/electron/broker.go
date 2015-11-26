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
	"./util"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"qpid.apache.org/amqp"
	"qpid.apache.org/electron"
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
	b := &broker{
		queues:    util.MakeQueues(*qsize),
		container: electron.NewContainer(""),
		acks:      make(chan electron.Outcome),
		sent:      make(chan sentMessage),
	}
	if err := b.run(); err != nil {
		log.Fatal(err)
	}
}

// State for the broker
type broker struct {
	queues    util.Queues           // A collection of queues.
	container electron.Container    // electron.Container manages AMQP connections.
	sent      chan sentMessage      // Channel to record sent messages.
	acks      chan electron.Outcome // Channel to receive the Outcome of sent messages.
}

// Record of a sent message and the queue it came from.
// If a message is rejected or not acknowledged due to a failure, we will put it back on the queue.
type sentMessage struct {
	m amqp.Message
	q util.Queue
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
		conn, err := listener.Accept()
		if err != nil {
			util.Debugf("Accept error: %v", err)
			continue
		}
		c, err := b.container.Connection(conn, electron.Server(), electron.AllowIncoming())
		if err != nil {
			util.Debugf("Connection error: %v", err)
			continue
		}
		cc := &connection{b, c}
		go cc.run() // Handle the connection
		util.Debugf("Accepted %v", c)
	}
}

// State for a broker connectoin
type connection struct {
	broker     *broker
	connection electron.Connection
}

// accept remotely-opened endpoints (Session, Sender and Receiver) on a connection
// and start goroutines to service them.
func (c *connection) run() {
	for in := range c.connection.Incoming() {
		switch in := in.(type) {

		case *electron.IncomingSender:
			if in.Source() == "" {
				in.Reject(fmt.Errorf("no source"))
			} else {
				go c.sender(in.Accept().(electron.Sender))
			}

		case *electron.IncomingReceiver:
			if in.Target() == "" {
				in.Reject(fmt.Errorf("no target"))
			} else {
				in.SetPrefetch(true)
				in.SetCapacity(*credit) // Pre-fetch up to credit window.
				go c.receiver(in.Accept().(electron.Receiver))
			}

		default:
			in.Accept() // Accept sessions unconditionally
		}
		util.Debugf("incoming: %v", in)
	}
	util.Debugf("incoming closed: %v", c.connection)
}

// receiver receives messages and pushes to a queue.
func (c *connection) receiver(receiver electron.Receiver) {
	q := c.broker.queues.Get(receiver.Target())
	for {
		if rm, err := receiver.Receive(); err == nil {
			util.Debugf("%v: received %v", receiver, util.FormatMessage(rm.Message))
			q <- rm.Message
			rm.Accept()
		} else {
			util.Debugf("%v error: %v", receiver, err)
			break
		}
	}
}

// sender pops messages from a queue and sends them.
func (c *connection) sender(sender electron.Sender) {
	q := c.broker.queues.Get(sender.Source())
	for {
		if sender.Error() != nil {
			util.Debugf("%v closed: %v", sender, sender.Error())
			return
		}
		select {

		case m := <-q:
			util.Debugf("%v: sent %v", sender, util.FormatMessage(m))
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
				util.Debugf("message %v put back, status %v, error %v",
					util.FormatMessage(sm.m), outcome.Status, outcome.Error)
			}
		}
	}
}
