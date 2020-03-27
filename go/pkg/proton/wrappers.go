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

// This file contains special-case wrapper functions or wrappers that don't follow
// the pattern of genwrap.go.

package proton

//#include <proton/codec.h>
//#include <proton/connection.h>
//#include <proton/delivery.h>
//#include <proton/event.h>
//#include <proton/link.h>
//#include <proton/link.h>
//#include <proton/object.h>
//#include <proton/sasl.h>
//#include <proton/session.h>
//#include <proton/transport.h>
//#include <stdlib.h>
import "C"

import (
	"fmt"
	"time"
	"unsafe"

	"github.com/apache/qpid-proton/go/pkg/amqp"
)

// TODO aconway 2015-05-05: Documentation for generated types.

// CHandle holds an unsafe.Pointer to a proton C struct, the C type depends on the
// Go type implementing this interface. For low level, at-your-own-risk use only.
type CHandle interface {
	// CPtr returns the unsafe C pointer, equivalent to a C void*.
	CPtr() unsafe.Pointer
}

// Incref increases the refcount of a proton value, which prevents the
// underlying C struct being freed until you call Decref().
//
// It can be useful to "pin" a proton value in memory while it is in use by
// goroutines other than the event loop goroutine. For example if you Incref() a
// Link, the underlying object is not freed when the link is closed, so means
// other goroutines can continue to safely use it as an index in a map or inject
// it into the event loop goroutine. There will of course be an error if you try
// to use a link after it is closed, but not a segmentation fault.
func Incref(c CHandle) {
	if p := c.CPtr(); p != nil {
		C.pn_incref(p)
	}
}

// Decref decreases the refcount of a proton value, freeing the underlying C
// struct if this is the last reference.  Only call this if you previously
// called Incref() for this value.
func Decref(c CHandle) {
	if p := c.CPtr(); p != nil {
		C.pn_decref(p)
	}
}

// Event is an AMQP protocol event.
type Event struct {
	pn         *C.pn_event_t
	eventType  EventType
	connection Connection
	transport  Transport
	session    Session
	link       Link
	delivery   Delivery
	injecter   Injecter
}

func makeEvent(pn *C.pn_event_t, injecter Injecter) Event {
	return Event{
		pn:         pn,
		eventType:  EventType(C.pn_event_type(pn)),
		connection: Connection{C.pn_event_connection(pn)},
		transport:  Transport{C.pn_event_transport(pn)},
		session:    Session{C.pn_event_session(pn)},
		link:       Link{C.pn_event_link(pn)},
		delivery:   Delivery{C.pn_event_delivery(pn)},
		injecter:   injecter,
	}
}
func (e Event) IsNil() bool            { return e.eventType == EventType(0) }
func (e Event) Type() EventType        { return e.eventType }
func (e Event) Connection() Connection { return e.connection }
func (e Event) Transport() Transport   { return e.transport }
func (e Event) Session() Session       { return e.session }
func (e Event) Link() Link             { return e.link }
func (e Event) Delivery() Delivery     { return e.delivery }
func (e Event) String() string         { return e.Type().String() }

// Injecter should not be used in a handler function, but it can be passed to
// other goroutines (via a channel or to a goroutine started by handler
// functions) to let them inject functions back into the handlers goroutine.
func (e Event) Injecter() Injecter { return e.injecter }

// Data is an intermediate form of decoded AMQP data.
type Data struct{ pn *C.pn_data_t }

func (d Data) Free()                { C.pn_data_free(d.pn) }
func (d Data) CPtr() unsafe.Pointer { return unsafe.Pointer(d.pn) }
func (d Data) Clear()               { C.pn_data_clear(d.pn) }
func (d Data) Rewind()              { C.pn_data_rewind(d.pn) }
func (d Data) Next()                { C.pn_data_next(d.pn) }
func (d Data) Error() error         { return PnError(C.pn_data_error(d.pn)) }
func (d Data) Empty() bool          { return C.pn_data_size(d.pn) == 0 }

func (d Data) String() string {
	str := C.pn_string(C.CString(""))
	defer C.pn_free(unsafe.Pointer(str))
	C.pn_inspect(unsafe.Pointer(d.pn), str)
	return C.GoString(C.pn_string_get(str))
}

// Unmarshal the value of d into value pointed at by ptr, see amqp.Unmarshal() for details
func (d Data) Unmarshal(ptr interface{}) error {
	d.Rewind()
	d.Next()
	err := amqp.UnmarshalUnsafe(d.CPtr(), ptr)
	return err
}

// Marshal the value v into d, see amqp.Marshal() for details
func (d Data) Marshal(v interface{}) error {
	d.Clear()
	return amqp.MarshalUnsafe(v, d.CPtr())
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

// Has is True if bits & state is non 0.
func (s State) Has(bits State) bool { return s&bits != 0 }

func (s State) LocalUninit() bool  { return s.Has(SLocalUninit) }
func (s State) LocalActive() bool  { return s.Has(SLocalActive) }
func (s State) LocalClosed() bool  { return s.Has(SLocalClosed) }
func (s State) RemoteUninit() bool { return s.Has(SRemoteUninit) }
func (s State) RemoteActive() bool { return s.Has(SRemoteActive) }
func (s State) RemoteClosed() bool { return s.Has(SRemoteClosed) }

// Return a State containing just the local flags
func (s State) Local() State { return State(s & C.PN_LOCAL_MASK) }

// Return a State containing just the remote flags
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
	// Human readable name
	String() string
	// Human readable endpoint type "sender-link", "session" etc.
	Type() string
}

// CloseError sets an error condition (if err != nil) on an endpoint and closes
// the endpoint if not already closed
func CloseError(e Endpoint, err error) {
	if err != nil && !e.Condition().IsSet() {
		e.Condition().SetError(err)
	}
	e.Close()
}

// EndpointError returns the remote error if there is one, the local error if not
// nil if there is no error.
func EndpointError(e Endpoint) error {
	err := e.RemoteCondition().Error()
	if err == nil {
		err = e.Condition().Error()
	}
	return err
}

const (
	Received uint64 = C.PN_RECEIVED
	Accepted        = C.PN_ACCEPTED
	Rejected        = C.PN_REJECTED
	Released        = C.PN_RELEASED
	Modified        = C.PN_MODIFIED
)

// SettleAs is equivalent to d.Update(disposition); d.Settle()
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

func (l Link) Connection() Connection { return l.Session().Connection() }

// Human-readable link description including name, source, target and direction.
func (l Link) String() string {
	switch {
	case l.IsNil():
		return fmt.Sprintf("<nil-link>")
	case l.IsSender():
		return fmt.Sprintf("%s(%s->%s)", l.Name(), l.Source().Address(), l.Target().Address())
	default:
		return fmt.Sprintf("%s(%s<-%s)", l.Name(), l.Target().Address(), l.Source().Address())
	}
}

func (l Link) Type() string {
	if l.IsSender() {
		return "sender-link"
	} else {
		return "receiver-link"
	}
}

// IsDrain calls pn_link_get_drain(), it conflicts with pn_link_drain() under the normal mapping.
func (l Link) IsDrain() bool {
	return bool(C.pn_link_get_drain(l.pn))
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

func (t Transport) String() string {
	return fmt.Sprintf("(Transport)(%p)", t.CPtr())
}

// Unique (per process) string identifier for a connection, useful for debugging.
func (c Connection) String() string {
	// Use the transport address to match the default transport logs from PN_TRACE.
	return fmt.Sprintf("(Connection)(%p)", c.Transport().CPtr())
}

func (c Connection) Type() string {
	return "connection"
}

// Head functions don't follow the normal naming conventions so missed by the generator.

func (c Connection) LinkHead(s State) Link {
	return Link{C.pn_link_head(c.pn, C.pn_state_t(s))}
}

func (c Connection) SessionHead(s State) Session {
	return Session{C.pn_session_head(c.pn, C.pn_state_t(s))}
}

func (c Connection) Links(state State) (links []Link) {
	for l := c.LinkHead(state); !l.IsNil(); l = l.Next(state) {
		links = append(links, l)
	}
	return
}

func (c Connection) Sessions(state State) (sessions []Session) {
	for s := c.SessionHead(state); !s.IsNil(); s = s.Next(state) {
		sessions = append(sessions, s)
	}
	return
}

// SetPassword takes []byte not string because it is impossible to erase a string
// from memory reliably. Proton will not keep the password in memory longer than
// needed, the caller should overwrite their copy on return.
//
// The password must not contain embedded nul characters, a trailing nul is ignored.
func (c Connection) SetPassword(password []byte) {
	if len(password) == 0 || password[len(password)-1] != 0 {
		password = append(password, 0) // Proton requires a terminating null.
	}
	C.pn_connection_set_password(c.pn, (*C.char)(unsafe.Pointer(&password[0])))
}

func (s Session) String() string {
	return fmt.Sprintf("(Session)(%p)", s.pn) // TODO aconway 2016-09-12: should print channel number.
}

func (s Session) Type() string { return "session" }

// Error returns an instance of amqp.Error or nil.
func (c Condition) Error() error {
	if c.IsNil() || !c.IsSet() {
		return nil
	}
	return amqp.Error{Name: c.Name(), Description: c.Description()}
}

// Set a Go error into a condition, converting to an amqp.Error using amqp.MakeError
func (c Condition) SetError(err error) {
	if err != nil {
		cond := amqp.MakeError(err)
		c.SetName(cond.Name)
		c.SetDescription(cond.Description)
	}
}

func (c Connection) Session() (Session, error) {
	s := Session{C.pn_session(c.pn)}
	if s.IsNil() {
		return s, Connection(c).Error()
	}
	return s, nil
}

// pnTime converts Go time.Time to Proton millisecond Unix time.
//
// Note: t.isZero() is converted to C.pn_timestamp_t(0) and vice-versa. These
// are used as "not set" sentinel values by the Go and Proton APIs, so it is
// better to conserve the "zeroness" even though they don't represent the same
// time instant.
//
func pnTime(t time.Time) (pnt C.pn_timestamp_t) {
	if !t.IsZero() {
		pnt = C.pn_timestamp_t(t.Unix()*1000 + int64(t.Nanosecond())/int64(time.Millisecond))
	}
	return
}

// goTime converts a pn_timestamp_t to a Go time.Time.
//
// Note: C.pn_timestamp_t(0) is converted to a zero time.Time and
// vice-versa. These are used as "not set" sentinel values by the Go and Proton
// APIs, so it is better to conserve the "zeroness" even though they don't
// represent the same time instant.
//
func goTime(pnt C.pn_timestamp_t) (t time.Time) {
	if pnt != 0 {
		t = time.Unix(int64(pnt/1000), int64(pnt%1000)*int64(time.Millisecond))
	}
	return
}

// Special treatment for Transport.Head, return value is unsafe.Pointer not string
func (t Transport) Head() unsafe.Pointer {
	return unsafe.Pointer(C.pn_transport_head(t.pn))
}

// Special treatment for Transport.Tail, return value is unsafe.Pointer not string
func (t Transport) Tail() unsafe.Pointer {
	return unsafe.Pointer(C.pn_transport_tail(t.pn))
}

// Special treatment for Transport.Push, takes []byte instead of char*, size
func (t Transport) Push(bytes []byte) int {
	return int(C.pn_transport_push(t.pn, (*C.char)(unsafe.Pointer(&bytes[0])), C.size_t(len(bytes))))
}

// Get the SASL object for the transport.
func (t Transport) SASL() SASL {
	return SASL{C.pn_sasl(t.pn)}
}

// Do we support extended SASL negotiation?
// All implementations of Proton support ANONYMOUS and EXTERNAL on both
// client and server sides and PLAIN on the client side.
//
// Extended SASL implememtations use an external library (Cyrus SASL)
// to support other mechanisms beyond these basic ones.
func SASLExtended() bool {
	return bool(C.pn_sasl_extended())
}
