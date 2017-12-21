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
	"reflect"
	"runtime"
	"testing"
	"time"
)

func fatalIf(t *testing.T, err error) {
	if err != nil {
		_, file, line, ok := runtime.Caller(1) // annotate with location of caller.
		if ok {
			_, file = path.Split(file)
		}
		t.Fatalf("(from %s:%d) %v", file, line, err)
	}
}

func errorIf(t *testing.T, err error) {
	if err != nil {
		_, file, line, ok := runtime.Caller(1) // annotate with location of caller.
		if ok {
			_, file = path.Split(file)
		}
		t.Errorf("(from %s:%d) %v", file, line, err)
	}
}

func checkEqual(want interface{}, got interface{}) error {
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("%#v != %#v", want, got)
	}
	return nil
}

// Start a server, return listening addr and channel for incoming Connections.
func newServer(t *testing.T, cont Container, opts ...ConnectionOption) (net.Addr, <-chan Connection) {
	listener, err := net.Listen("tcp4", "") // For systems with ipv6 disabled
	fatalIf(t, err)
	addr := listener.Addr()
	ch := make(chan Connection)
	go func() {
		conn, err := listener.Accept()
		c, err := cont.Connection(conn, append([]ConnectionOption{Server()}, opts...)...)
		fatalIf(t, err)
		ch <- c
	}()
	return addr, ch
}

// Open a client connection and session, return the session.
func newClient(t *testing.T, cont Container, addr net.Addr, opts ...ConnectionOption) Session {
	conn, err := net.Dial(addr.Network(), addr.String())
	fatalIf(t, err)
	// Don't  bother checking error here, it's an async error so it's racy to do so anyway.
	// Let caller use Sync() or catch it on first use.
	c, _ := cont.Connection(conn, opts...)
	sn, _ := c.Session()
	return sn
}

// Return client and server ends of the same connection.
func newClientServerOpts(t *testing.T, copts []ConnectionOption, sopts []ConnectionOption) (client Session, server Connection) {
	addr, ch := newServer(t, NewContainer("test-server"), sopts...)
	client = newClient(t, NewContainer("test-client"), addr, copts...)
	return client, <-ch
}

// Return client and server ends of the same connection.
func newClientServer(t *testing.T) (client Session, server Connection) {
	return newClientServerOpts(t, nil, nil)
}

// Close client and server
func closeClientServer(client Session, server Connection) {
	client.Connection().Close(nil)
	server.Close(nil)
}

// Send a message one way with a client sender and server receiver, verify ack.
func TestClientSendServerReceive(t *testing.T) {
	nLinks := 3
	nMessages := 3

	rchan := make(chan Receiver, nLinks)
	client, server := newClientServer(t)
	go func() {
		for in := range server.Incoming() {
			switch in := in.(type) {
			case *IncomingReceiver:
				in.SetCapacity(1)
				in.SetPrefetch(false)
				rchan <- in.Accept().(Receiver)
			default:
				in.Accept()
			}
		}
	}()

	defer func() { closeClientServer(client, server) }()

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
			// Client send
			ack := make(chan Outcome, 1)
			sendDone := make(chan struct{})
			go func() {
				defer close(sendDone)
				m := amqp.NewMessageWith(fmt.Sprintf("foobar%v-%v", i, j))
				var err error
				s[i].SendAsync(m, ack, "testing")
				if err != nil {
					t.Fatal(err)
				}
			}()

			// Server receive
			rm, err := r[i].Receive()
			if err != nil {
				t.Fatal(err)
			}
			if want, got := interface{}(fmt.Sprintf("foobar%v-%v", i, j)), rm.Message.Body(); want != got {
				t.Errorf("%#v != %#v", want, got)
			}

			// Should not be acknowledged on client yet
			<-sendDone
			select {
			case <-ack:
				t.Errorf("unexpected ack")
			default:
			}

			// Server send ack
			if err := rm.Reject(); err != nil {
				t.Error(err)
			}
			// Client get ack.
			if a := <-ack; a.Value != "testing" || a.Error != nil || a.Status != Rejected {
				t.Error("unexpected ack: ", a.Status, a.Error, a.Value)
			}
		}
	}
}

func TestClientReceiver(t *testing.T) {
	nMessages := 3
	client, server := newClientServer(t)
	go func() {
		for in := range server.Incoming() {
			switch in := in.(type) {
			case *IncomingSender:
				s := in.Accept().(Sender)
				go func() {
					for i := int32(0); i < int32(nMessages); i++ {
						out := s.SendSync(amqp.NewMessageWith(i))
						if out.Error != nil {
							t.Error(out.Error)
							return
						}
					}
					s.Close(nil)
				}()
			default:
				in.Accept()
			}
		}
	}()

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
	client, server := newClientServer(t)
	go func() {
		for i := range server.Incoming() {
			switch i := i.(type) {
			case *IncomingReceiver:
				i.SetCapacity(1)
				i.SetPrefetch(false)
				rchan <- i.Accept().(Receiver) // Issue credit only on receive
			default:
				i.Accept()
			}
		}
	}()
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
	if err := snd.SendSyncTimeout(m, 0).Error; err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	if err := snd.SendSyncTimeout(m, short).Error; err != Timeout { // No credit, expect timeout.
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
	ack := make(chan Outcome)
	snd.SendAsyncTimeout(m, ack, nil, short)
	// Disposition should timeout
	select {
	case <-ack:
		t.Errorf("want Timeout got %#v", ack)
	case <-time.After(short):
	}

	// Receive and accept
	rm, err := rcv.ReceiveTimeout(long)
	if err != nil {
		t.Fatal(err)
	}
	if err := rm.Accept(); err != nil {
		t.Fatal(err)
	}
	// Sender get ack
	if a := <-ack; a.Status != Accepted || a.Error != nil {
		t.Errorf("want (accepted, nil) got %#v", a)
	}
}

// A server that returns the opposite end of each client link via channels.
type pairs struct {
	t        *testing.T
	client   Session
	server   Connection
	rchan    chan Receiver
	schan    chan Sender
	capacity int
	prefetch bool
}

func newPairs(t *testing.T, capacity int, prefetch bool) *pairs {
	p := &pairs{t: t, rchan: make(chan Receiver, 1), schan: make(chan Sender, 1)}
	p.client, p.server = newClientServer(t)
	go func() {
		for i := range p.server.Incoming() {
			switch i := i.(type) {
			case *IncomingReceiver:
				i.SetCapacity(capacity)
				i.SetPrefetch(prefetch)
				p.rchan <- i.Accept().(Receiver)
			case *IncomingSender:
				p.schan <- i.Accept().(Sender)
			default:
				i.Accept()
			}
		}
	}()
	return p
}

func (p *pairs) close() {
	closeClientServer(p.client, p.server)
}

// Return a client sender and server receiver
func (p *pairs) senderReceiver() (Sender, Receiver) {
	snd, err := p.client.Sender()
	fatalIf(p.t, err)
	rcv := <-p.rchan
	return snd, rcv
}

// Return a client receiver and server sender
func (p *pairs) receiverSender() (Receiver, Sender) {
	rcv, err := p.client.Receiver()
	fatalIf(p.t, err)
	snd := <-p.schan
	return rcv, snd
}

type result struct {
	label string
	err   error
	value interface{}
}

func (r result) String() string { return fmt.Sprintf("%v(%v)", r.err, r.label) }

func doSend(snd Sender, results chan result) {
	err := snd.SendSync(amqp.NewMessage()).Error
	results <- result{"send", err, nil}
}

func doReceive(rcv Receiver, results chan result) {
	msg, err := rcv.Receive()
	results <- result{"receive", err, msg}
}

func doDisposition(ack <-chan Outcome, results chan result) {
	results <- result{"disposition", (<-ack).Error, nil}
}

// Senders get credit immediately if receivers have prefetch set
func TestSendReceivePrefetch(t *testing.T) {
	pairs := newPairs(t, 1, true)
	s, r := pairs.senderReceiver()
	s.SendAsyncTimeout(amqp.NewMessage(), nil, nil, time.Second) // Should not block for credit.
	if _, err := r.Receive(); err != nil {
		t.Error(err)
	}
}

// Senders do not get credit till Receive() if receivers don't have prefetch
func TestSendReceiveNoPrefetch(t *testing.T) {
	pairs := newPairs(t, 1, false)
	s, r := pairs.senderReceiver()
	done := make(chan struct{}, 1)
	go func() {
		s.SendAsyncTimeout(amqp.NewMessage(), nil, nil, time.Second) // Should block for credit.
		close(done)
	}()
	select {
	case <-done:
		t.Errorf("send should be blocked on credit")
	default:
		if _, err := r.Receive(); err != nil {
			t.Error(err)
		} else {
			<-done
		} // Should be unblocked now
	}
}

// Test that closing Links interrupts blocked link functions.
func TestLinkCloseInterrupt(t *testing.T) {
	want := amqp.Error{Name: "x", Description: "all bad"}
	pairs := newPairs(t, 1, false)
	results := make(chan result) // Collect expected errors

	// Note closing the link does not interrupt Send() calls, the AMQP spec says
	// that deliveries can be settled after the link is closed.

	// Receiver.Close() interrupts Receive()
	snd, rcv := pairs.senderReceiver()
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
	want := amqp.Error{Name: "x", Description: "bad"}
	pairs := newPairs(t, 1, true)
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := pairs.senderReceiver()
	go doSend(snd, results)

	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}
	rcv, snd = pairs.receiverSender()
	go doReceive(rcv, results)

	snd, rcv = pairs.senderReceiver()
	ack := snd.SendWaitable(amqp.NewMessage())
	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}
	go doDisposition(ack, results)

	pairs.server.Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			t.Errorf("want %v got %v", want, r)
		}
	}
}

// Test closing the client end of the connection.
func TestConnectionCloseInterrupt2(t *testing.T) {
	want := amqp.Error{Name: "x", Description: "bad"}
	pairs := newPairs(t, 1, true)
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := pairs.senderReceiver()
	go doSend(snd, results)
	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}

	rcv, snd = pairs.receiverSender()
	go doReceive(rcv, results)

	snd, rcv = pairs.senderReceiver()
	ack := snd.SendWaitable(amqp.NewMessage())
	go doDisposition(ack, results)

	pairs.client.Connection().Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			t.Errorf("want %v got %v", want, r.err)
		}
	}
}

func heartbeat(c Connection) time.Duration {
	return c.(*connection).engine.Transport().RemoteIdleTimeout()
}

func TestHeartbeat(t *testing.T) {
	client, server := newClientServerOpts(t,
		[]ConnectionOption{Heartbeat(102 * time.Millisecond)},
		nil)
	defer closeClientServer(client, server)

	var serverHeartbeat time.Duration

	go func() {
		for in := range server.Incoming() {
			switch in := in.(type) {
			case *IncomingConnection:
				serverHeartbeat = in.Heartbeat()
				in.AcceptConnection(Heartbeat(101 * time.Millisecond))
			default:
				in.Accept()
			}
		}
	}()

	// Freeze the server to stop it sending heartbeats.
	unfreeze := make(chan bool)
	defer close(unfreeze)
	freeze := func() error { return server.(*connection).engine.Inject(func() { <-unfreeze }) }

	fatalIf(t, client.Sync())
	errorIf(t, checkEqual(101*time.Millisecond, heartbeat(client.Connection())))
	errorIf(t, checkEqual(102*time.Millisecond, serverHeartbeat))
	errorIf(t, client.Connection().Error())

	// Freeze the server for less than a heartbeat
	fatalIf(t, freeze())
	time.Sleep(50 * time.Millisecond)
	unfreeze <- true
	// Make sure server is still responding.
	s, err := client.Sender()
	errorIf(t, err)
	errorIf(t, s.Sync())

	// Freeze the server till the client times out the connection
	fatalIf(t, freeze())
	select {
	case <-client.Done():
		if amqp.ResourceLimitExceeded != client.Error().(amqp.Error).Name {
			t.Error("bad timeout error:", client.Error())
		}
	case <-time.After(400 * time.Millisecond):
		t.Error("connection failed to time out")
	}

	unfreeze <- true // Unfreeze the server
	<-server.Done()
	if amqp.ResourceLimitExceeded != server.Error().(amqp.Error).Name {
		t.Error("bad timeout error:", server.Error())
	}
}
