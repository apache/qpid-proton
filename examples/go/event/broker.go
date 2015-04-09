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
// This is a simple AMQP broker implemented using the event-handler interface.
//
// It maintains a set of named in-memory queues of messages. Clients can send
// messages to queues or subscribe to receive messages from them.
//
//

package main

import (
	"container/list"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"os"
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/event"
)

// panicIf is simplistic error handling for example code, not recommended practice.
func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

// queue is a structure representing a queue.
type queue struct {
	name      string              // Name of queue
	messages  *list.List          // List of event.Message
	consumers map[event.Link]bool // Set of consumer links
}

func newQueue(name string) *queue {
	debug.Printf("Create queue %s\n", name)
	return &queue{name, list.New(), make(map[event.Link]bool)}
}

func (q *queue) subscribe(link event.Link) {
	debug.Printf("Subscribe to %s\n", q.name)
	q.consumers[link] = true
}

func (q *queue) unsubscribe(link event.Link) {
	debug.Printf("Unsubscribe from %s\n", q.name)
	delete(q.consumers, link)
}

func (q *queue) empty() bool {
	return len(q.consumers) == 0 && q.messages.Len() == 0
}

func (q *queue) publish(message proton.Message) {
	debug.Printf("Push to %s: %#v\n", q.name, message)
	q.messages.PushBack(message)
	q.dispatch()
}

func (q *queue) dispatchTo(link event.Link) bool {
	if q.messages.Len() != 0 && link.Credit() > 0 {
		message := q.messages.Front().Value.(proton.Message)
		debug.Printf("Pop from %s: %#v\n", q.name, message)
		// The first return parameter is an event.Delivery.
		// The Deliver can be used to track message status, e.g. so we can re-delver on failure.
		// This demo broker doesn't do that.
		_, err := message.Send(link)
		panicIf(err)
		q.messages.Remove(q.messages.Front())
		return true
	}
	return false
}

func (q *queue) dispatch() (dispatched bool) {
	for c, _ := range q.consumers {
		dispatched = dispatched || q.dispatchTo(c)
	}
	return
}

// broker implements event.MessagingHandler and reacts to events by moving messages on or off queues.
type broker struct {
	queues map[string]*queue
	pumps  map[*event.Pump]struct{} // Set of running event pumps (i.e. connections)
}

func newBroker() *broker {
	return &broker{queues: make(map[string]*queue), pumps: make(map[*event.Pump]struct{})}
}

func (b *broker) getQueue(name string) *queue {
	q := b.queues[name]
	if q == nil {
		q = newQueue(name)
		b.queues[name] = q
	}
	return q
}

func (b *broker) Handle(t event.MessagingEventType, e event.Event) error {
	switch t {

	case event.MLinkOpening:
		if e.Link().IsSender() {
			// FIXME aconway 2015-03-23: handle dynamic consumers
			b.getQueue(e.Link().RemoteSource().Address()).subscribe(e.Link())
		}

	case event.MLinkClosing:
		if e.Link().IsSender() {
			q := b.getQueue(e.Link().RemoteSource().Address())
			q.unsubscribe(e.Link())
			if q.empty() {
				delete(b.queues, q.name)
			}
		}

	case event.MSendable:
		b.getQueue(e.Link().RemoteSource().Address()).dispatchTo(e.Link())

	case event.MMessage:
		m, err := proton.EventMessage(e)
		panicIf(err)
		b.getQueue(e.Link().RemoteTarget().Address()).publish(m)
	}
	return nil
}

func (b *broker) listen(addr string) (err error) {
	// Use the standard Go "net" package to listen for connections.
	info.Printf("Listening on %s\n", addr)
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	defer listener.Close()
	for {
		conn, err := listener.Accept()
		if err != nil {
			info.Printf("Accept error: %s\n", err)
			continue
		}
		info.Printf("Accepted connection %s<-%s\n", conn.LocalAddr(), conn.RemoteAddr())
		pump, err := event.NewPump(conn, event.NewMessagingDelegator(b))
		panicIf(err)
		pump.Server()
		b.pumps[pump] = struct{}{}
		go pump.Run()
	}
}

var addr = flag.String("addr", ":amqp", "Listening address")
var quiet = flag.Bool("quiet", false, "Don't print informational messages")
var debugFlag = flag.Bool("debug", false, "Print debugging messages")
var info, debug *log.Logger

func output(enable bool) io.Writer {
	if enable {
		return os.Stdout
	} else {
		return ioutil.Discard
	}
}

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `
Usage: %s [queue ...]
A simple broker. Queues are created automatically for sender or receiver addrsses.
`, os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	debug = log.New(output(*debugFlag), "debug: ", log.Ltime)
	info = log.New(output(!*quiet), "info: ", log.Ltime)
	b := newBroker()
	err := b.listen(*addr)
	if err != nil {
		fmt.Println(err)
		os.Exit(1)
	}
}
