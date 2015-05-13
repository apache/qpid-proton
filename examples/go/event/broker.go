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
	"path"
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/event"
	"sync"
)

// Command-line flags
var addr = flag.String("addr", ":amqp", "Listening address")
var verbose = flag.Int("verbose", 1, "Output level, 0 means none, higher means more")
var full = flag.Bool("full", false, "Print full message not just body.")

func main() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `
Usage: %s
A simple broker-like demo. Queues are created automatically for sender or receiver addrsses.
`, os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	b := newBroker()
	err := b.listen(*addr)
	fatalIf(err)
}

// queue is a structure representing a queue.
type queue struct {
	name      string              // Name of queue
	messages  *list.List          // List of event.Message
	consumers map[event.Link]bool // Set of consumer links
}

type logLink event.Link // Wrapper to print links in format for logging

func (ll logLink) String() string {
	l := event.Link(ll)
	return fmt.Sprintf("%s[%p]", l.Name(), l.Session().Connection().Pump())
}

func (q *queue) subscribe(link event.Link) {
	debug.Printf("link %s subscribed to queue %s", logLink(link), q.name)
	q.consumers[link] = true
}

func (q *queue) unsubscribe(link event.Link) {
	debug.Printf("link %s unsubscribed from queue %s", logLink(link), q.name)
	delete(q.consumers, link)
}

func (q *queue) empty() bool {
	return len(q.consumers) == 0 && q.messages.Len() == 0
}

func (q *queue) push(context *event.Pump, message proton.Message) {
	q.messages.PushBack(message)
	q.pop(context)
}

func (q *queue) popTo(context *event.Pump, link event.Link) bool {
	if q.messages.Len() != 0 && link.Credit() > 0 {
		message := q.messages.Remove(q.messages.Front()).(proton.Message)
		debug.Printf("link %s <- queue %s: %s", logLink(link), q.name, formatMessage{message})
		// The first return parameter is an event.Delivery.
		// The Deliver can be used to track message status, e.g. so we can re-delver on failure.
		// This demo broker doesn't do that.
		linkPump := link.Session().Connection().Pump()
		if context == linkPump {
			if context == nil {
				log.Fatal("pop in nil context")
			}
			link.Send(message) // link is in the current pump, safe to call Send() direct
		} else {
			linkPump.Inject <- func() { // Inject to link's pump
				link.Send(message) // FIXME aconway 2015-05-04: error handlig
			}
		}
		return true
	}
	return false
}

func (q *queue) pop(context *event.Pump) (popped bool) {
	for c, _ := range q.consumers {
		popped = popped || q.popTo(context, c)
	}
	return
}

// broker implements event.MessagingHandler and reacts to events by moving messages on or off queues.
type broker struct {
	queues map[string]*queue
	lock   sync.Mutex // FIXME aconway 2015-05-04: un-golike, better broker coming...
}

func newBroker() *broker {
	return &broker{queues: make(map[string]*queue)}
}

func (b *broker) getQueue(name string) *queue {
	q := b.queues[name]
	if q == nil {
		debug.Printf("Create queue %s", name)
		q = &queue{name, list.New(), make(map[event.Link]bool)}
		b.queues[name] = q
	}
	return q
}

func (b *broker) unsubscribe(l event.Link) {
	if l.IsSender() {
		q := b.queues[l.RemoteSource().Address()]
		if q != nil {
			q.unsubscribe(l)
			if q.empty() {
				debug.Printf("Delete queue %s", q.name)
				delete(b.queues, q.name)
			}
		}
	}
}

func (b *broker) HandleMessagingEvent(t event.MessagingEventType, e event.Event) error {
	// FIXME aconway 2015-05-04: locking is un-golike, better example coming soon.
	// Needed because the same handler is used for multiple connections concurrently
	// and the queue data structures are not thread safe.
	b.lock.Lock()
	defer b.lock.Unlock()

	switch t {

	case event.MLinkOpening:
		if e.Link().IsSender() {
			q := b.getQueue(e.Link().RemoteSource().Address())
			q.subscribe(e.Link())
		}

	case event.MLinkDisconnected, event.MLinkClosing:
		b.unsubscribe(e.Link())

	case event.MSendable:
		q := b.getQueue(e.Link().RemoteSource().Address())
		q.popTo(e.Connection().Pump(), e.Link())

	case event.MMessage:
		m, err := event.DecodeMessage(e)
		fatalIf(err)
		qname := e.Link().RemoteTarget().Address()
		debug.Printf("link %s -> queue %s: %s", logLink(e.Link()), qname, formatMessage{m})
		b.getQueue(qname).push(e.Connection().Pump(), m)
	}
	return nil
}

func (b *broker) listen(addr string) (err error) {
	// Use the standard Go "net" package to listen for connections.
	info.Printf("Listening on %s", addr)
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	defer listener.Close()
	for {
		conn, err := listener.Accept()
		if err != nil {
			info.Printf("Accept error: %s", err)
			continue
		}
		pump, err := event.NewPump(conn, event.NewMessagingDelegator(b))
		fatalIf(err)
		info.Printf("Accepted %s[%p]", pump, pump)
		pump.Server()
		go func() {
			pump.Run()
			if pump.Error == nil {
				info.Printf("Closed %s", pump)
			} else {
				info.Printf("Closed %s: %s", pump, pump.Error)
			}
		}()
	}
}

// Logging
func logger(prefix string, level int, w io.Writer) *log.Logger {
	if *verbose >= level {
		return log.New(w, prefix, 0)
	}
	return log.New(ioutil.Discard, "", 0)
}

var info, debug *log.Logger

func init() {
	flag.Parse()
	name := path.Base(os.Args[0])
	log.SetFlags(0)
	log.SetPrefix(fmt.Sprintf("%s: ", name))                      // Log errors on stderr.
	info = logger(fmt.Sprintf("%s: ", name), 1, os.Stdout)        // Log info on stdout.
	debug = logger(fmt.Sprintf("%s debug: ", name), 2, os.Stderr) // Log debug on stderr.
}

// Simple error handling for demo.
func fatalIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

type formatMessage struct{ m proton.Message }

func (fm formatMessage) String() string {
	if *full {
		return fmt.Sprintf("%#v", fm.m)
	} else {
		return fmt.Sprintf("%#v", fm.m.Body())
	}
}
