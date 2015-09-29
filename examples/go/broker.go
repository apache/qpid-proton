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
// This is a simple AMQP broker implemented using the concurrent interface.
//
// It maintains a set of named in-memory queues of messages. Clients can send
// messages to queues or subscribe to receive messages from them.
//
//

package main

import (
	"./util"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"qpid.apache.org/proton/concurrent"
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

func main() {
	flag.Usage = usage
	flag.Parse()
	b := newBroker()
	err := b.listen(*addr)
	util.ExitIf(err)
}

type broker struct {
	container concurrent.Container
	queues    util.QueueMap
}

func newBroker() *broker {
	return &broker{
		container: concurrent.NewContainer(""),
		queues:    util.MakeQueueMap(),
	}
}

// Listen for incoming connections
func (b *broker) listen(addr string) (err error) {
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	log.Printf("Listening on %s\n", listener.Addr())
	defer listener.Close()
	for {
		conn, err := listener.Accept()
		if err != nil {
			return err
		}
		c, err := b.container.Connection(conn)
		if err != nil {
			return err
		}
		// Make this a server connection. Must be done before Open()
		c.Server() // Server-side protocol negotiation.
		c.Listen() // Enable remotely-opened endpoints.
		if err := c.Open(); err != nil {
			return err
		}
		util.Debugf("accept %s\n", c)
		// Accept remotely-opened endpoints on the connection
		go b.accept(c)
	}
}

// accept remotely-opened endpoints (Session, Sender and Receiver)
func (b *broker) accept(c concurrent.Connection) {
	for ep, err := c.Accept(); err == nil; ep, err = c.Accept() {
		switch ep := ep.(type) {
		case concurrent.Session:
			util.Debugf("accept session %s\n", ep)
			ep.Open()
		case concurrent.Sender:
			util.Debugf("accept sender %s\n", ep)
			ep.Open()
			go b.sender(ep)
		case concurrent.Receiver:
			util.Debugf("accept receiver %s\n", ep)
			ep.SetCapacity(100, true) // Pre-fetch 100 messages
			ep.Open()
			go b.receiver(ep)
		}
	}
}

// sender pops from a the queue in the sender's Source address and send messages.
func (b *broker) sender(sender concurrent.Sender) {
	qname := sender.Settings().Source
	if qname == "" {
		log.Printf("invalid consumer, no source address: %s", sender)
		return
	}
	q := b.queues.Get(qname)
	for {
		m := <-q.Pop
		if m == nil {
			break
		}
		if sm, err := sender.Send(m); err == nil {
			sm.Forget() // FIXME aconway 2015-09-24: Ignore acknowledgements
			util.Debugf("send %s: %s\n", sender, util.FormatMessage(m))
		} else {
			util.Debugf("send error %s: %s\n", sender, err)
			q.Putback <- m
			break
		}
	}
}

func (b *broker) receiver(receiver concurrent.Receiver) {
	qname := receiver.Settings().Target
	if qname == "" {
		log.Printf("invalid producer, no target address: %s", receiver)
		return
	}
	q := b.queues.Get(qname)
	for {
		if rm, err := receiver.Receive(); err == nil {
			util.Debugf("recv %s: %s\n", receiver, util.FormatMessage(rm.Message))
			q.Push <- rm.Message
			rm.Accept()
		} else {
			util.Debugf("recv error %s: %s\n", receiver, err)
			break
		}
	}
}
