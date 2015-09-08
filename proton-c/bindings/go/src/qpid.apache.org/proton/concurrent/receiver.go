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

package concurrent

import (
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/amqp"
	"qpid.apache.org/proton/internal"
	"time"
)

type ReceiverSettings struct {
	LinkSettings

	// Capacity is the number of messages that the receiver can buffer locally.
	// If unset (zero) it will be set to 1.
	Capacity int

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
	Prefetch bool
}

// Receiver is a Link that receives messages.
//
type Receiver interface {
	Link

	// SetCapacity sets the Capacity and Prefetch (see ReceiverSettings) It may
	// may called before Open() on an accepted receiver, it cannot be changed once
	// the receiver is Open().
	SetCapacity(capacity int, prefetch bool)

	// Receive blocks until a message is available or until the Receiver is closed
	// and has no more buffered messages.
	Receive() (ReceivedMessage, error)

	// ReceiveTimeout is like Receive but gives up after timeout, see Timeout.
	ReceiveTimeout(timeout time.Duration) (ReceivedMessage, error)
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
	// Set in Setup()
	capacity int
	prefetch bool

	// Set in Open()
	buffer chan ReceivedMessage
	policy policy
}

func newReceiver(l link) Receiver { return &receiver{link: l} }

func (r *receiver) SetCapacity(capacity int, prefetch bool) {
	r.setPanicIfOpen()
	if capacity < 1 {
		capacity = 1
	}
	r.capacity = capacity
	r.prefetch = prefetch
}

// Accept and open an incoming receiver.
func (r *receiver) Open() error {
	if r.capacity == 0 {
		r.SetCapacity(1, false)
	}
	if r.prefetch {
		r.policy = &prefetchPolicy{}
	} else {
		r.policy = &noPrefetchPolicy{}
	}
	err := r.engine().InjectWait(func() error {
		err := r.open()
		if err == nil {
			r.buffer = make(chan ReceivedMessage, r.capacity)
			r.handler().addLink(r.eLink, r)
		}
		return err
	})
	return r.setError(err)
}

// call in proton goroutine
func (r *receiver) credit() (buffered, credit, capacity int) {
	return len(r.buffer), r.eLink.Credit(), cap(r.buffer)
}

func (r *receiver) Capacity() int { return cap(r.buffer) }

func (r *receiver) Receive() (rm ReceivedMessage, err error) {
	return r.ReceiveTimeout(Forever)
}

func (r *receiver) ReceiveTimeout(timeout time.Duration) (rm ReceivedMessage, err error) {
	internal.Assert(r.buffer != nil, "Receiver is not open: %s", r)
	r.policy.Pre(r)
	defer func() { r.policy.Post(r, err) }()
	rmi, ok, timedout := timedReceive(r.buffer, timeout)
	switch {
	case timedout:
		return ReceivedMessage{}, Timeout
	case !ok:
		return ReceivedMessage{}, r.Error()
	default:
		return rmi.(ReceivedMessage), nil
	}
}

// Called in proton goroutine
func (r *receiver) handleDelivery(delivery proton.Delivery) {
	// FIXME aconway 2015-09-24: how can this happen if we are remote closed?
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
	r.closeError(err)
	close(r.buffer)
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
