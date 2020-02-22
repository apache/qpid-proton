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
	"testing"
	"time"

	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

// Send a message one way with a client sender and server receiver, verify ack.
func TestClientSender(t *testing.T) {
	p := newPipe(t, nil, nil)
	defer func() { p.close() }()

	nLinks := 3
	nMessages := 3

	s := make([]Sender, nLinks)
	r := make([]Receiver, nLinks)
	for i := 0; i < nLinks; i++ {
		s[i], r[i] = p.sender(Target(fmt.Sprintf("foo%d", i)))
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
				test.FatalIf(t, err)
			}()

			// Server receive
			rm, err := r[i].Receive()
			test.FatalIf(t, err)
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
			if err = rm.Reject(); err != nil {
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
	p := newPipe(t, nil, nil)
	defer func() { p.close() }()
	r, s := p.receiver(Source("foo"), Capacity(nMessages), Prefetch(true))
	go func() {
		for i := 0; i < nMessages; i++ { // Server sends
			out := s.SendSync(amqp.NewMessageWith(int32(i)))
			test.FatalIf(t, out.Error)
		}
	}()
	for i := 0; i < nMessages; i++ { // Client receives
		rm, err := r.Receive()
		test.FatalIf(t, err)
		test.ErrorIf(t, test.Differ(int32(i), rm.Message.Body()))
		test.ErrorIf(t, rm.Accept())
	}
}

// Test timeout versions of waiting functions.
func TestTimeouts(t *testing.T) {
	p := newPipe(t, nil, nil)
	defer func() { p.close() }()
	snd, rcv := p.sender(Target("test"))

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
	if _, err := rcv.ReceiveTimeout(0); err != Timeout { // No credit, expect timeout.
		t.Error("want Timeout got", err)
	}
	// Test receive with timeout
	if _, err := rcv.ReceiveTimeout(short); err != Timeout { // No credit, expect timeout.
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
	if err = rm.Accept(); err != nil {
		t.Fatal(err)
	}
	// Sender get ack
	if a := <-ack; a.Status != Accepted || a.Error != nil {
		t.Errorf("want (accepted, nil) got %#v", a)
	}
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
	p := newPipe(t, nil, nil)
	p.prefetch = true
	s, r := p.sender()
	s.SendAsyncTimeout(amqp.NewMessage(), nil, nil, time.Second) // Should not block for credit.
	if _, err := r.Receive(); err != nil {
		t.Error(err)
	}
}

// Senders do not get credit till Receive() if receivers don't have prefetch
func TestSendReceiveNoPrefetch(t *testing.T) {
	p := newPipe(t, nil, nil)
	p.prefetch = false
	s, r := p.sender()
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
	p := newPipe(t, nil, nil)
	results := make(chan result) // Collect expected errors

	// Note closing the link does not interrupt Send() calls, the AMQP spec says
	// that deliveries can be settled after the link is closed.

	// Receiver.Close() interrupts Receive()
	snd, rcv := p.sender()
	go doReceive(rcv, results)
	rcv.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}

	// Remote Sender.Close() interrupts Receive()
	snd, rcv = p.sender()
	go doReceive(rcv, results)
	snd.Close(want)
	if r := <-results; want != r.err {
		t.Errorf("want %#v got %#v", want, r)
	}
}

// Test closing the server end of a connection.
func TestConnectionCloseInterrupt1(t *testing.T) {
	want := amqp.Error{Name: "x", Description: "bad"}
	p := newSocketPair(t, nil, nil)
	p.prefetch = true
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := p.sender()
	go doSend(snd, results)

	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}
	rcv, snd = p.receiver()
	go doReceive(rcv, results)

	snd, rcv = p.sender()
	ack := snd.SendWaitable(amqp.NewMessage())
	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}
	go doDisposition(ack, results)

	p.server.Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			t.Errorf("want %v got %v", want, r)
		}
	}
}

// Test closing the client end of the connection.
func TestConnectionCloseInterrupt2(t *testing.T) {
	want := amqp.Error{Name: "x", Description: "bad"}
	p := newSocketPair(t, nil, nil)
	p.prefetch = true
	results := make(chan result) // Collect expected errors

	// Connection.Close() interrupts Send, Receive, Disposition.
	snd, rcv := p.sender()
	go doSend(snd, results)
	if _, err := rcv.Receive(); err != nil {
		t.Error("receive", err)
	}

	rcv, snd = p.receiver()
	go doReceive(rcv, results)

	snd, rcv = p.sender()
	ack := snd.SendWaitable(amqp.NewMessage())
	go doDisposition(ack, results)

	p.client.Connection().Close(want)
	for i := 0; i < 3; i++ {
		if r := <-results; want != r.err {
			t.Errorf("want %v got %v", want, r.err)
		}
	}
}

func TestHeartbeat(t *testing.T) {
	p := newSocketPair(t,
		[]ConnectionOption{Heartbeat(102 * time.Millisecond)},
		[]ConnectionOption{Heartbeat(101 * time.Millisecond)})
	defer func() { p.close() }()

	// Function to freeze the server to stop it sending heartbeats.
	unfreeze := make(chan bool)
	defer close(unfreeze)
	freeze := func() error { return p.server.(*connection).engine.Inject(func() { <-unfreeze }) }

	test.FatalIf(t, p.client.Sync())
	test.ErrorIf(t, test.Differ(101*time.Millisecond, p.client.Connection().Heartbeat()))
	test.ErrorIf(t, test.Differ(102*time.Millisecond, p.server.Heartbeat()))

	// Freeze the server for less than a heartbeat
	test.FatalIf(t, freeze())
	time.Sleep(5 * time.Millisecond)
	unfreeze <- true
	// Make sure server is still responding.
	s, _ := p.sender()
	test.ErrorIf(t, s.Sync())

	// Freeze the server till the p.client times out the connection
	test.FatalIf(t, freeze())
	select {
	case <-p.client.Done():
		if amqp.ResourceLimitExceeded != p.client.Error().(amqp.Error).Name {
			t.Error("bad timeout error:", p.client.Error())
		}
	case <-time.After(1400 * time.Millisecond):
		t.Error("connection failed to time out")
	}

	unfreeze <- true // Unfreeze the server
	<-p.server.Done()
	if p.server.Error() == nil {
		t.Error("expected server side  time-out or connection abort error")
	}
}
