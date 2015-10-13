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
	b := &broker{util.MakeQueues(*qsize), electron.NewContainer("")}
	if err := b.run(); err != nil {
		log.Fatal(err)
	}
}

// State for the broker
type broker struct {
	queues    util.Queues
	container electron.Container
}

// Listens for connections and starts an electron.Connection for each one.
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
			util.Debugf("Accept error: %v", err)
			continue
		}
		c, err := b.container.Connection(conn, electron.Server(), electron.Accepter(b.accept))
		if err != nil {
			util.Debugf("Connection error: %v", err)
			continue
		}
		util.Debugf("Accepted %v", c)
	}
}

// accept remotely-opened endpoints (Session, Sender and Receiver)
// and start goroutines to service them.
func (b *broker) accept(i electron.Incoming) {
	switch i := i.(type) {
	case *electron.IncomingSender:
		go b.sender(i.AcceptSender())
	case *electron.IncomingReceiver:
		go b.receiver(i.AcceptReceiver(100, true)) // Pre-fetch 100 messages
	default:
		i.Accept()
	}
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
			util.Debugf("%s send: %s", sender, util.FormatMessage(m))
		} else {
			util.Debugf("%s error: %s", sender, err)
			q <- m // Put it back on the queue.
			return
		}
	}
}

// receiver receives messages and pushes to a queue.
func (b *broker) receiver(receiver electron.Receiver) {
	q := b.queues.Get(receiver.Target())
	for {
		if rm, err := receiver.Receive(); err == nil {
			util.Debugf("%s: received %s", receiver, util.FormatMessage(rm.Message))
			q <- rm.Message
			rm.Accept()
		} else {
			util.Debugf("%s error: %s", receiver, err)
			break
		}
	}
}
