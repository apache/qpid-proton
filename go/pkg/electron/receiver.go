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
	"time"

	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/proton"
)

// Receiver is a Link that receives messages.
//
type Receiver interface {
	Endpoint
	LinkSettings

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

// Receiver implementation
type receiver struct {
	link
	buffer  chan ReceivedMessage
	callers int
}

func (r *receiver) Capacity() int  { return cap(r.buffer) }
func (r *receiver) Prefetch() bool { return r.prefetch }

// Call in proton goroutine
func newReceiver(ls linkSettings) *receiver {
	r := &receiver{link: link{linkSettings: ls}}
	r.endpoint.init(r.link.pLink.String())
	if r.capacity < 1 {
		r.capacity = 1
	}
	r.buffer = make(chan ReceivedMessage, r.capacity)
	r.handler().addLink(r.pLink, r)
	r.link.pLink.Open()
	if r.prefetch {
		r.flow(r.maxFlow())
	}
	return r
}

// Call in proton goroutine. Max additional credit we can request.
func (r *receiver) maxFlow() int { return cap(r.buffer) - len(r.buffer) - r.pLink.Credit() }

func (r *receiver) flow(credit int) {
	if credit > 0 {
		r.pLink.Flow(credit)
	}
}

// Inject flow check per-caller call when prefetch is off.
// Called with inc=1 at start of call, inc = -1 at end
func (r *receiver) caller(inc int) {
	_ = r.engine().Inject(func() {
		r.callers += inc
		need := r.callers - (len(r.buffer) + r.pLink.Credit())
		max := r.maxFlow()
		if need > max {
			need = max
		}
		r.flow(need)
	})
}

// Inject flow top-up if prefetch is enabled
func (r *receiver) flowTopUp() {
	if r.prefetch {
		_ = r.engine().Inject(func() { r.flow(r.maxFlow()) })
	}
}

func (r *receiver) Receive() (rm ReceivedMessage, err error) {
	return r.ReceiveTimeout(Forever)
}

func (r *receiver) ReceiveTimeout(timeout time.Duration) (rm ReceivedMessage, err error) {
	if r.buffer == nil {
		panic(fmt.Errorf("Receiver is not open: %s", r))
	}
	if !r.prefetch { // Per-caller flow control
		select { // Check for immediate availability, avoid caller() inject
		case rm2, ok := <-r.buffer:
			if ok {
				rm = rm2
			} else {
				err = r.Error()
			}
			return
		default: // Not immediately available, inject caller() counts
			r.caller(+1)
			defer r.caller(-1)
		}
	}
	rmi, err := timedReceive(r.buffer, timeout)
	switch err {
	case nil:
		r.flowTopUp()
		rm = rmi.(ReceivedMessage)
	case Closed:
		err = r.Error()
	}
	return
}

// Called in proton goroutine on MMessage event.
func (r *receiver) message(delivery proton.Delivery) {
	if r.pLink.State().RemoteClosed() {
		localClose(r.pLink, r.pLink.RemoteCondition().Error())
		return
	}
	if delivery.HasMessage() {
		bytes, err := delivery.MessageBytes()
		var m amqp.Message
		if err == nil {
			m = amqp.NewMessage()
			err = r.session.connection.mc.Decode(m, bytes)
		}
		if err != nil {
			localClose(r.pLink, err)
			return
		}
		r.pLink.Advance()
		if r.pLink.Credit() < 0 {
			localClose(r.pLink, fmt.Errorf("received message in excess of credit limit"))
		} else {
			// We never issue more credit than cap(buffer) so this will not block.
			r.buffer <- ReceivedMessage{m, delivery, r}
		}
	}
}

func (r *receiver) closed(err error) error {
	e := r.link.closed(err)
	if r.buffer != nil {
		close(r.buffer)
	}
	return e
}

// ReceivedMessage contains an amqp.Message and allows the message to be acknowledged.
type ReceivedMessage struct {
	// Message is the received message.
	Message amqp.Message

	pDelivery proton.Delivery
	receiver  Receiver
}

// Acknowledge a ReceivedMessage with the given delivery status.
func (rm *ReceivedMessage) acknowledge(status uint64) error {
	return rm.receiver.(*receiver).engine().Inject(func() {
		// Deliveries are valid as long as the connection is, unless settled.
		rm.pDelivery.SettleAs(uint64(status))
	})
}

// Accept tells the sender that we take responsibility for processing the message.
func (rm *ReceivedMessage) Accept() error { return rm.acknowledge(proton.Accepted) }

// Reject tells the sender we consider the message invalid and unusable.
func (rm *ReceivedMessage) Reject() error { return rm.acknowledge(proton.Rejected) }

// Release tells the sender we will not process the message but some other
// receiver might.
func (rm *ReceivedMessage) Release() error { return rm.acknowledge(proton.Released) }

// IncomingReceiver is sent on the Connection.Incoming() channel when there is
// an incoming request to open a receiver link.
type IncomingReceiver struct {
	incoming
	linkSettings
}

func newIncomingReceiver(sn *session, pLink proton.Link) *IncomingReceiver {
	return &IncomingReceiver{
		incoming:     makeIncoming(pLink),
		linkSettings: makeIncomingLinkSettings(pLink, sn),
	}
}

// SetCapacity sets the capacity of the incoming receiver, call before Accept()
func (in *IncomingReceiver) SetCapacity(capacity int) { in.capacity = capacity }

// SetPrefetch sets the pre-fetch mode of the incoming receiver, call before Accept()
func (in *IncomingReceiver) SetPrefetch(prefetch bool) { in.prefetch = prefetch }

// Accept accepts an incoming receiver endpoint
func (in *IncomingReceiver) Accept() Endpoint {
	return in.accept(func() Endpoint { return newReceiver(in.linkSettings) })
}
