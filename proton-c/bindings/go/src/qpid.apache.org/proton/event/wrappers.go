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

package event

//#include <proton/codec.h>
//#include <proton/connection.h>
//#include <proton/delivery.h>
//#include <proton/event.h>
//#include <proton/transport.h>
//#include <proton/link.h>
import "C"

import (
	"unsafe"
)

// Data holds a pointer to decoded AMQP data, proton.marshal/unmarshal to access it as Go data.
type Data struct{ pn *C.pn_data_t }

func NewData(p unsafe.Pointer) Data { return Data{(*C.pn_data_t)(p)} }

func (d Data) Free()                   { C.pn_data_free(d.pn) }
func (d Data) Pointer() unsafe.Pointer { return unsafe.Pointer(d.pn) }
func (d Data) Clear()                  { C.pn_data_clear(d.pn) }
func (d Data) Rewind()                 { C.pn_data_rewind(d.pn) }
func (d Data) Error() error            { return pnError(C.pn_data_error(d.pn)) }

// State holds the state flags for an AMQP endpoint.
type State byte

func (s State) LocalUninitialized() bool { return s&C.PN_LOCAL_UNINIT != 0 }
func (s State) LocalActive() bool        { return s&C.PN_LOCAL_ACTIVE != 0 }
func (s State) LocalOpen() bool          { return s&C.PN_LOCAL_ACTIVE != 0 }
func (s State) LocalClosed() bool        { return s&C.PN_LOCAL_CLOSED != 0 }

func (s State) RemoteUninitialized() bool { return s&C.PN_REMOTE_UNINIT != 0 }
func (s State) RemoteActive() bool        { return s&C.PN_REMOTE_ACTIVE != 0 }
func (s State) RemoteOpen() bool          { return s&C.PN_REMOTE_ACTIVE != 0 }
func (s State) RemoteClosed() bool        { return s&C.PN_REMOTE_CLOSED != 0 }

// Return a State containig just the local flags
func (s State) Local() State { return State(s & C.PN_LOCAL_MASK) }

// Return a State containig just the remote flags
func (s State) Remote() State { return State(s & C.PN_REMOTE_MASK) }

// Endpoint is the common interface for Connection, Link and Session.
type Endpoint interface {
	State() State
	Open()
	Close()
	Condition() Condition
	RemoteCondition() Condition
}

// Disposition types
const (
	Received uint64 = C.PN_RECEIVED
	Accepted        = C.PN_ACCEPTED
	Rejected        = C.PN_REJECTED
	Released        = C.PN_RELEASED
	Modified        = C.PN_MODIFIED
)

// SettleAs is equivalent to d.Update(disposition); d.Settle()
// It is a no-op if e does not have a delivery.
func (d Delivery) SettleAs(disposition uint64) {
	d.Update(disposition)
	d.Settle()
}

// Accept accepts and settles a delivery.
func (d Delivery) Accept() { d.SettleAs(Accepted) }

// Reject rejects and settles a delivery
func (d Delivery) Reject() { d.SettleAs(Rejected) }

// Release releases and settles a delivery
// If delivered is true the delivery count for the message will be increased.
func (d Delivery) Release(delivered bool) {
	if delivered {
		d.SettleAs(Modified)
	} else {
		d.SettleAs(Released)
	}
}

type Transport struct{ pn *C.pn_transport_t }
type DeliveryTag struct{ pn C.pn_delivery_tag_t } // FIXME aconway 2015-03-25: convert to string

func (l Link) Recv(buf []byte) int {
	if len(buf) == 0 {
		return 0
	}
	return int(C.pn_link_recv(l.pn, (*C.char)(unsafe.Pointer(&buf[0])), C.size_t(len(buf))))
}

func (l Link) Send(bytes []byte) int {
	return int(C.pn_link_send(l.pn, cPtr(bytes), cLen(bytes)))
}

func pnTag(tag string) C.pn_delivery_tag_t {
	bytes := []byte(tag)
	return C.pn_dtag(cPtr(bytes), cLen(bytes))
}

func (l Link) Delivery(tag string) Delivery {
	return Delivery{C.pn_delivery(l.pn, pnTag(tag))}
}

func cPtr(b []byte) *C.char {
	if len(b) == 0 {
		return nil
	}
	return (*C.char)(unsafe.Pointer(&b[0]))
}

func cLen(b []byte) C.size_t {
	return C.size_t(len(b))
}
