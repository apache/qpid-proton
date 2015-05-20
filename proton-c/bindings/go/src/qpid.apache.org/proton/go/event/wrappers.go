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
//#include <proton/session.h>
//#include <proton/session.h>
//#include <proton/delivery.h>
//#include <proton/link.h>
//#include <proton/event.h>
//#include <proton/transport.h>
//#include <proton/link.h>
//#include <stdlib.h>
import "C"

import (
	"fmt"
	"qpid.apache.org/proton/go/internal"
	"unsafe"
)

// FIXME aconway 2015-05-05: Documentation for generated types.

// Event is an AMQP protocol event.
type Event struct {
	pn         *C.pn_event_t
	eventType  EventType
	connection Connection
	session    Session
	link       Link
	delivery   Delivery
}

func makeEvent(pn *C.pn_event_t) Event {
	return Event{
		pn:         pn,
		eventType:  EventType(C.pn_event_type(pn)),
		connection: Connection{C.pn_event_connection(pn)},
		session:    Session{C.pn_event_session(pn)},
		link:       Link{C.pn_event_link(pn)},
		delivery:   Delivery{C.pn_event_delivery(pn)},
	}
}
func (e Event) IsNil() bool            { return e.eventType == EventType(0) }
func (e Event) Type() EventType        { return e.eventType }
func (e Event) Connection() Connection { return e.connection }
func (e Event) Session() Session       { return e.session }
func (e Event) Link() Link             { return e.link }
func (e Event) Delivery() Delivery     { return e.delivery }
func (e Event) String() string         { return e.Type().String() }

// Data holds a pointer to decoded AMQP data.
// Use amqp.marshal/unmarshal to access it as Go data types.
//
type Data struct{ pn *C.pn_data_t }

func NewData(p unsafe.Pointer) Data { return Data{(*C.pn_data_t)(p)} }

func (d Data) Free()                   { C.pn_data_free(d.pn) }
func (d Data) Pointer() unsafe.Pointer { return unsafe.Pointer(d.pn) }
func (d Data) Clear()                  { C.pn_data_clear(d.pn) }
func (d Data) Rewind()                 { C.pn_data_rewind(d.pn) }
func (d Data) Error() error {
	return internal.PnError(unsafe.Pointer(C.pn_data_error(d.pn)))
}

// State holds the state flags for an AMQP endpoint.
type State byte

const (
	SLocalUninit  State = C.PN_LOCAL_UNINIT
	SLocalActive        = C.PN_LOCAL_ACTIVE
	SLocalClosed        = C.PN_LOCAL_CLOSED
	SRemoteUninit       = C.PN_REMOTE_UNINIT
	SRemoteActive       = C.PN_REMOTE_ACTIVE
	SRemoteClosed       = C.PN_REMOTE_CLOSED
)

// Is is True if bits & state is non 0.
func (s State) Is(bits State) bool { return s&bits != 0 }

// Return a State containig just the local flags
func (s State) Local() State { return State(s & C.PN_LOCAL_MASK) }

// Return a State containig just the remote flags
func (s State) Remote() State { return State(s & C.PN_REMOTE_MASK) }

// Endpoint is the common interface for Connection, Link and Session.
type Endpoint interface {
	// State is the open/closed state.
	State() State
	// Open an endpoint.
	Open()
	// Close an endpoint.
	Close()
	// Condition holds a local error condition.
	Condition() Condition
	// RemoteCondition holds a remote error condition.
	RemoteCondition() Condition
}

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

// FIXME aconway 2015-05-05: don't expose DeliveryTag as a C pointer, just as a String?

type DeliveryTag struct{ pn C.pn_delivery_tag_t }

func (t DeliveryTag) String() string { return C.GoStringN(t.pn.start, C.int(t.pn.size)) }

func (l Link) Recv(buf []byte) int {
	if len(buf) == 0 {
		return 0
	}
	return int(C.pn_link_recv(l.pn, (*C.char)(unsafe.Pointer(&buf[0])), C.size_t(len(buf))))
}

func (l Link) SendBytes(bytes []byte) int {
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

func (s Session) Sender(name string) Link {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return Link{C.pn_sender(s.pn, cname)}
}

func (s Session) Receiver(name string) Link {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return Link{C.pn_receiver(s.pn, cname)}
}

func joinId(a, b interface{}) string {
	return fmt.Sprintf("%s/%s", a, b)
}

// Pump associated with this connection.
func (c Connection) Pump() *Pump { return pumps[c.pn] }

// Unique (per process) string identifier for a connection, useful for debugging.
func (c Connection) String() string { return pumps[c.pn].String() }

// Head functions don't follow the normal naming conventions so missed by the generator.

func (c Connection) LinkHead(s State) Link {
	return Link{C.pn_link_head(c.pn, C.pn_state_t(s))}
}

func (c Connection) SessionHead(s State) Session {
	return Session{C.pn_session_head(c.pn, C.pn_state_t(s))}
}

// Unique (per process) string identifier for a session, including connection identifier.
func (s Session) String() string {
	return joinId(s.Connection(), fmt.Sprintf("%p", s.pn))
}

// Unique (per process) string identifier for a link, inlcuding session identifier.
func (l Link) String() string {
	return joinId(l.Session(), l.Name())
}

// Error returns an error interface corresponding to Condition.
func (c Condition) Error() error {
	if c.IsNil() {
		return nil
	} else {
		return fmt.Errorf("%s: %s", c.Name(), c.Description())
	}
}

// SetIfUnset sets name and description on a condition if it is not already set.
func (c Condition) SetIfUnset(name, description string) {
	if !c.IsSet() {
		c.SetName(name)
		c.SetDescription(description)
	}
}

func (c Connection) Session() (Session, error) {
	s := Session{C.pn_session(c.pn)}
	if s.IsNil() {
		return s, Connection(c).Error()
	}
	return s, nil
}
