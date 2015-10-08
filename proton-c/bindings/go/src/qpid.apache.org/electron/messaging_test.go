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

package electron

import (
	"fmt"
	"net"
	"path"
	"qpid.apache.org/amqp"
	"runtime"
	"testing"
	"time"
)

func fatalIf(t *testing.T, err error) {
	if err != nil { // FIXME aconway 2015-10-07:
		_, file, line, ok := runtime.Caller(1) // annotate with location of caller.
		if ok {
			_, file = path.Split(file)
		}
		t.Fatalf("(from %s:%d) %v", file, line, err)
	}
}

// Start a server, return listening addr and channel for incoming Connection.
func newServer(t *testing.T, cont Container, accept func(Endpoint) error) (net.Addr, <-chan Connection) {
	listener, err := net.Listen("tcp", "")
	fatalIf(t, err)
	addr := listener.Addr()
	ch := make(chan Connection)
	go func() {
		conn, err := listener.Accept()
		c, err := cont.Connection(conn)
		fatalIf(t, err)
		c.Server()
		c.Listen(accept)
		fatalIf(t, c.Open())
		ch <- c
	}()
	return addr, ch
}

// Return open an client connection and session, return the session.
func newClient(t *testing.T, cont Container, addr net.Addr) Session {
	conn, err := net.Dial(addr.Network(), addr.String())
	fatalIf(t, err)
	c, err := cont.Connection(conn)
	fatalIf(t, err)
	c.Open()
	sn, err := c.Session()
	fatalIf(t, err)
	return sn
}

// Return client and server ends of the same connection.
func newClientServer(t *testing.T, accept func(Endpoint) error) (client Session, server Connection) {
	addr, ch := newServer(t, NewContainer(""), accept)
	client = newClient(t, NewContainer(""), addr)
	return client, <-ch
}

// Close client and server
func closeClientServer(client Session, server Connection) {
	client.Connection().Close(nil)
	server.Close(nil)
}

// Send a message one way with a client sender and server receiver, verify ack.
func TestClientSendServerReceive(t *testing.T) {
	timeout := time.Second * 2
	nLinks := 3
	nMessages := 3

	rchan := make(chan Receiver, nLinks)
	client, server := newClientServer(t, func(ep Endpoint) error {
		if r, ok := ep.(Receiver); ok {
			r.SetCapacity(1, false)
			rchan <- r
		}
		return nil
	})

	defer func() {
		closeClientServer(client, server)
	}()

	s := make([]Sender, nLinks)
	for i := 0; i < nLinks; i++ {
		var err error
		s[i], err = client.Sender(Target(fmt.Sprintf("foo%d", i)))
		if err != nil {
			t.Fatal(err)
		}
	}
	r := make([]Receiver, nLinks)
	for i := 0; i < nLinks; i++ {
		r[i] = <-rchan
	}

	for i := 0; i < nLinks; i++ {
		for j := 0; j < nMessages; j++ {
			var sm SentMessage

			// Client send
			sendDone := make(chan struct{})
			go func() {
				defer close(sendDone)
				m := amqp.NewMessageWith(fmt.Sprintf("foobar%v-%v", i, j))
				var err error
				sm, err = s[i].Send(m)
				if err != nil {
					t.Fatal(err)
				}
			}()

			// Server recieve
			rm, err := r[i].Receive()
			if err != nil {
				t.Fatal(err)
			}
			if want, got := interface{}(fmt.Sprintf("foobar%v-%v", i, j)), rm.Message.Body(); want != got {
				t.Errorf("%#v != %#v", want, got)
			}

			// Should not be acknowledged on client yet
			<-sendDone
			if d, err := sm.DispositionTimeout(0); err != Timeout || NoDisposition != d {
				t.Errorf("want [no-disposition/timeout] got [%v/%v]", d, err)
			}
			// Server ack
			if err := rm.Acknowledge(Rejected); err != nil {
				t.Error(err)
			}
			// Client get ack.
			if d, err := sm.DispositionTimeout(timeout); err != nil || Rejected != d {
				t.Errorf("want [rejected/nil] got [%v/%v]", d, err)
			}
		}
	}
}

func TestClientReceiver(t *testing.T) {
	nMessages := 3
	client, server := newClientServer(t, func(ep Endpoint) error {
		if s, ok := ep.(Sender); ok {
			go func() {
				for i := int32(0); i < int32(nMessages); i++ {
					sm, err := s.Send(amqp.NewMessageWith(i))
					if err != nil {
						t.Error(err)
						return
					} else {
						sm.Disposition() // Sync send.
					}
				}
				s.Close(nil)
			}()
		}
		return nil
	})

	r, err := client.Receiver(Source("foo"))
	if err != nil {
		t.Fatal(err)
	}
	for i := int32(0); i < int32(nMessages); i++ {
		rm, err := r.Receive()
		if err != nil {
			if err != Closed {
				t.Error(err)
			}
			break
		}
		if err := rm.Accept(); err != nil {
			t.Error(err)
		}
		if b, ok := rm.Message.Body().(int32); !ok || b != i {
			t.Errorf("want %v, true got %v, %v", i, b, ok)
		}
	}
	server.Close(nil)
	client.Connection().Close(nil)
}

// Test timeout versions of waiting functions.
func TestTimeouts(t *testing.T) {
	var err error
	rchan := make(chan Receiver, 1)
	client, server := newClientServer(t, func(ep Endpoint) error {
		if r, ok := ep.(Receiver); ok {
			r.SetCapacity(1, false) // Issue credit only on receive
			rchan <- r
		}
		return nil
	})
	defer func() { closeClientServer(client, server) }()

	// Open client sender
	snd, err := client.Sender(Target("test"))
	if err != nil {
		t.Fatal(err)
	}
	rcv := <-rchan

	// Test send with timeout
	short := time.Millisecond
	long := time.Second
	m := amqp.NewMessage()
	if _, err = snd.SendTimeout(m, 0); err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	if _, err = snd.SendTimeout(m, short); err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	// Test receive with timeout
	if _, err = rcv.ReceiveTimeout(0); err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	// Test receive with timeout
	if _, err = rcv.ReceiveTimeout(short); err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	// There is now a credit on the link due to receive
	sm, err := snd.SendTimeout(m, long)
	if err != nil {
		t.Fatal(err)
	}
	// Disposition should timeout
	if _, err = sm.DispositionTimeout(long); err != Timeout {
		t.Error("want Timeout got", err)
	}
	if _, err = sm.DispositionTimeout(short); err != Timeout {
		t.Error("want Timeout got", err)
	}
	// Receive and accept
	rm, err := rcv.ReceiveTimeout(long)
	if err != nil {
		t.Fatal(err)
	}
	rm.Accept()
	// Sender get ack
	d, err := sm.DispositionTimeout(long)
	if err != nil || d != Accepted {
		t.Errorf("want (rejected, nil) got (%v, %v)", d, err)
	}
}

// clientServer that returns sender/receiver pairs at opposite ends of link.
type pairs struct {
	t      *testing.T
	client Session
	server Connection
	rchan  chan Receiver
	schan  chan Sender
}

func newPairs(t *testing.T) *pairs {
	p := &pairs{t: t, rchan: make(chan Receiver, 1), schan: make(chan Sender, 1)}
	p.client, p.server = newClientServer(t, func(ep Endpoint) error {
		switch ep := ep.(type) {
		case Receiver:
			ep.SetCapacity(1, false)
			p.rchan <- ep
		case Sender:
			p.schan <- ep
		}
		return nil
	})
	return p
}

func (p *pairs) close() {
	closeClientServer(p.client, p.server)
}

func (p *pairs) senderReceiver() (Sender, Receiver) {
	snd, err := p.client.Sender()
	fatalIf(p.t, err)
	rcv := <-p.rchan
	return snd, rcv
}

func (p *pairs) receiverSender() (Receiver, Sender) {
	rcv, err := p.client.Receiver()
	fatalIf(p.t, err)
	snd := <-p.schan
	return rcv, snd
}

type result struct {
	label string
	err   error
}

func (r result) String() string { return fmt.Sprintf("%s(%s)", r.err, r.label) }

func doSend(snd Sender, results chan result) {
	_, err := snd.Send(amqp.NewMessage())
	results <- result{"send", err}
}

func doReceive(rcv Receiver, results chan result) {
	_, err := rcv.Receive()
	results <- result{"receive", err}
}

func doDisposition(sm SentMessage, results chan result) {
	_, err := sm.Disposition()
	results <- result{"disposition", err}
}

// Test that closing Links interrupts blocked link functions.
func TestLinkCloseInterrupt(t *testing.T) {
	want := amqp.Errorf("x", "all bad")
	pairs := newPairs(t)
	results := make(chan result) // Collect expected errors

	// Sender.Close() interrupts Send()
	snd, rcv := pairs.senderReceiver()
	go doSend(snd, results)
	snd.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}

	// Remote Receiver.Close() interrupts Send()
	snd, rcv = pairs.senderReceiver()
	go doSend(snd, results)
	rcv.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}

	// Receiver.Close() interrupts Receive()
	snd, rcv = pairs.senderReceiver()
	go doReceive(rcv, results)
	rcv.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}

	// Remote Sender.Close() interrupts Receive()
	snd, rcv = pairs.senderReceiver()
	go doReceive(rcv, results)
	snd.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}
}

// Test closing the server end of a connection.
func TestConnectionCloseInterrupt1(t *testing.T) {
	want := amqp.Errorf("x", "bad")
	pairs := newPairs(t)
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := pairs.senderReceiver()
	go doReceive(rcv, results)
	sm, err := snd.Send(amqp.NewMessage())
	fatalIf(t, err)
	go doDisposition(sm, results)
	snd, rcv = pairs.senderReceiver()
	go doSend(snd, results)
	rcv, snd = pairs.receiverSender()
	go doReceive(rcv, results)
	pairs.server.Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			// TODO aconway 2015-10-06: Not propagating the correct error, seeing nil and EOF.
			t.Logf("want %v got %v", want, r)
		}
	}
}

// Test closing the client end of the connection.
func TestConnectionCloseInterrupt2(t *testing.T) {
	want := amqp.Errorf("x", "bad")
	pairs := newPairs(t)
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := pairs.senderReceiver()
	go doReceive(rcv, results)
	sm, err := snd.Send(amqp.NewMessage())
	fatalIf(t, err)
	go doDisposition(sm, results)
	snd, rcv = pairs.senderReceiver()
	go doSend(snd, results)
	rcv, snd = pairs.receiverSender()
	go doReceive(rcv, results)
	pairs.client.Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			// TODO aconway 2015-10-06: Not propagating the correct error, seeing nil.
			t.Logf("want %v got %v", want, r)
		}
	}
}
