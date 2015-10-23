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
	"qpid.apache.org/amqp"
	"qpid.apache.org/internal"
	"qpid.apache.org/proton"
	"time"
)

// Receiver is a Link that receives messages.
//
type Receiver interface {
	Link

	// Receive blocks until a message is available or until the Receiver is closed
	// and has no more buffered messages.
	Receive() (ReceivedMessage, error)

	// ReceiveTimeout is like Receive but gives up after timeout, see Timeout.
	//
	// Note that that if Prefetch is false, after a Timeout the credit issued by
	// Receive remains on the link. It will be used by the next call to Receive.
	ReceiveTimeout(timeout time.Duration) (ReceivedMessage, error)

	// Prefetch==true means the Receiver will automatically issue credit to the
	// remote sender to keep its buffer as full as possible, i.e. it will
	// "pre-fetch" messages independently of the application calling
	// Receive(). This gives good throughput for applications that handle a
	// continuous stream of messages. Larger capacity may improve throughput, the
	// optimal value depends on the characteristics of your application.
	//
	// Prefetch==false means the Receiver will issue only issue credit when you
	// call Receive(), and will only issue enough credit to satisfy the calls
	// actually made. This gives lower throughput but will not fetch any messages
	// in advance. It is good for synchronous applications that need to evaluate
	// each message before deciding whether to receive another. The
	// request-response pattern is a typical example.  If you make concurrent
	// calls to Receive with pre-fetch disabled, you can improve performance by
	// setting the capacity close to the expected number of concurrent calls.
	//
	Prefetch() bool

	// Capacity is the size (number of messages) of the local message buffer
	// These are messages received but not yet returned to the application by a call to Receive()
	Capacity() int
}

// Flow control policy for a receiver.
type policy interface {
	// Called at the start of Receive() to adjust credit before fetching a message.
	Pre(*receiver)
	// Called after Receive() has received a message from Buffer() before it returns.
	// Non-nil error means no message was received because of an error.
	Post(*receiver, error)
}

type prefetchPolicy struct{}

func (p prefetchPolicy) Flow(r *receiver) {
	r.engine().Inject(func() {
		_, _, max := r.credit()
		if max > 0 {
			r.eLink.Flow(max)
		}
	})
}
func (p prefetchPolicy) Pre(r *receiver) { p.Flow(r) }
func (p prefetchPolicy) Post(r *receiver, err error) {
	if err == nil {
		p.Flow(r)
	}
}

type noPrefetchPolicy struct{ waiting int }

func (p noPrefetchPolicy) Flow(r *receiver) { // Not called in proton goroutine
	r.engine().Inject(func() {
		len, credit, max := r.credit()
		add := p.waiting - (len + credit)
		if add > max {
			add = max // Don't overflow
		}
		if add > 0 {
			r.eLink.Flow(add)
		}
	})
}
func (p noPrefetchPolicy) Pre(r *receiver) { p.waiting++; p.Flow(r) }
func (p noPrefetchPolicy) Post(r *receiver, err error) {
	p.waiting--
	if err == nil {
		p.Flow(r)
	}
}

// Receiver implementation
type receiver struct {
	link
	buffer chan ReceivedMessage
	policy policy
}

func newReceiver(l link) *receiver {
	r := &receiver{link: l}
	if r.capacity < 1 {
		r.capacity = 1
	}
	if r.prefetch {
		r.policy = &prefetchPolicy{}
	} else {
		r.policy = &noPrefetchPolicy{}
	}
	r.buffer = make(chan ReceivedMessage, r.capacity)
	r.handler().addLink(r.eLink, r)
	r.link.open()
	return r
}

// call in proton goroutine.
func (r *receiver) credit() (buffered, credit, max int) {
	return len(r.buffer), r.eLink.Credit(), cap(r.buffer) - len(r.buffer)
}

func (r *receiver) Capacity() int  { return cap(r.buffer) }
func (r *receiver) Prefetch() bool { return r.prefetch }

func (r *receiver) Receive() (rm ReceivedMessage, err error) {
	return r.ReceiveTimeout(Forever)
}

func (r *receiver) ReceiveTimeout(timeout time.Duration) (rm ReceivedMessage, err error) {
	internal.Assert(r.buffer != nil, "Receiver is not open: %s", r)
	r.policy.Pre(r)
	defer func() { r.policy.Post(r, err) }()
	rmi, err := timedReceive(r.buffer, timeout)
	switch err {
	case Timeout:
		return ReceivedMessage{}, Timeout
	case Closed:
		return ReceivedMessage{}, r.Error()
	default:
		return rmi.(ReceivedMessage), nil
	}
}

// Called in proton goroutine on MMessage event.
func (r *receiver) message(delivery proton.Delivery) {
	if r.eLink.State().RemoteClosed() {
		localClose(r.eLink, r.eLink.RemoteCondition().Error())
		return
	}
	if delivery.HasMessage() {
		m, err := delivery.Message()
		if err != nil {
			localClose(r.eLink, err)
			return
		}
		internal.Assert(m != nil)
		r.eLink.Advance()
		if r.eLink.Credit() < 0 {
			localClose(r.eLink, internal.Errorf("received message in excess of credit limit"))
		} else {
			// We never issue more credit than cap(buffer) so this will not block.
			r.buffer <- ReceivedMessage{m, delivery, r}
		}
	}
}

func (r *receiver) closed(err error) {
	r.link.closed(err)
	if r.buffer != nil {
		close(r.buffer)
	}
}

// ReceivedMessage contains an amqp.Message and allows the message to be acknowledged.
type ReceivedMessage struct {
	// Message is the received message.
	Message amqp.Message

	eDelivery proton.Delivery
	receiver  Receiver
}

// Acknowledge a ReceivedMessage with the given disposition code.
func (rm *ReceivedMessage) Acknowledge(disposition Disposition) error {
	return rm.receiver.(*receiver).engine().InjectWait(func() error {
		// Settle doesn't return an error but if the receiver is broken the settlement won't happen.
		rm.eDelivery.SettleAs(uint64(disposition))
		return rm.receiver.Error()
	})
}

// Accept is short for Acknowledge(Accpeted)
func (rm *ReceivedMessage) Accept() error { return rm.Acknowledge(Accepted) }

// Reject is short for Acknowledge(Rejected)
func (rm *ReceivedMessage) Reject() error { return rm.Acknowledge(Rejected) }

// IncomingReceiver is passed to the accept() function given to Connection.Listen()
// when there is an incoming request for a receiver link.
type IncomingReceiver struct {
	incomingLink
}

// Link provides information about the incoming link.
func (i *IncomingReceiver) Link() Link { return i }

// AcceptReceiver sets Capacity and Prefetch of the accepted Receiver.
func (i *IncomingReceiver) AcceptReceiver(capacity int, prefetch bool) Receiver {
	i.capacity = capacity
	i.prefetch = prefetch
	return i.Accept().(Receiver)
}

func (i *IncomingReceiver) Accept() Endpoint {
	i.accepted = true
	return newReceiver(i.link)
}
