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
	if err := newBroker().run(); err != nil {
		log.Fatal(err)
	}
}

type broker struct {
	queues    util.Queues
	container electron.Container
}

func newBroker() *broker {
	return &broker{util.MakeQueues(*qsize), electron.NewContainer("")}
}

func (b *broker) run() (err error) {
	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		return err
	}
	defer listener.Close()
	fmt.Printf("Listening on %s\n", listener.Addr())
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

// connection creates a new AMQP connection for a net.Conn.
func (b *broker) connection(conn net.Conn) error {
	c, err := b.container.Connection(conn)
	if err != nil {
		return err
	}
	c.Server()         // Enable server-side protocol negotiation.
	c.Listen(b.accept) // Call accept() for remotely-opened endpoints.
	if err := c.Open(); err != nil {
		return err
	}
	util.Debugf("Accepted %s", c)
	return nil
}

// accept remotely-opened endpoints (Session, Sender and Receiver)
// and start goroutines to service them.
func (b *broker) accept(ep electron.Endpoint) error {
	switch ep := ep.(type) {
	case electron.Sender:
		util.Debugf("%s opened", ep)
		go b.sender(ep)
	case electron.Receiver:
		util.Debugf("%s opened", ep)
		ep.SetCapacity(100, true) // Pre-fetch 100 messages
		go b.receiver(ep)
	}
	return nil
}

// sender pops messages from a queue and sends them.
func (b *broker) sender(sender electron.Sender) {
	q := b.queues.Get(sender.Source())
	for {
		m, ok := <-q
		if !ok { // Queue closed
			return
		}
		if err := sender.SendForget(m); err == nil {
			util.Debugf("send %s: %s", sender, util.FormatMessage(m))
		} else {
			util.Debugf("send error %s: %s", sender, err)
			q <- m // Put it back on the queue.
			break
		}
	}
}

// receiver receives messages and pushes to the queue named by the receivers's
// Target address
func (b *broker) receiver(receiver electron.Receiver) {
	q := b.queues.Get(receiver.Target())
	for {
		if rm, err := receiver.Receive(); err == nil {
			util.Debugf("%s: received %s", receiver, util.FormatMessage(rm.Message))
			q <- rm.Message
			rm.Accept()
		} else {
			util.Debugf("%s: error %s", receiver, err)
			break
		}
	}
}
