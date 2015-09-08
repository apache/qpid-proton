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
	"qpid.apache.org/proton/amqp"
	"qpid.apache.org/proton/internal"
	"reflect"
	"time"
	"unsafe"
)

// TODO aconway 2015-05-05: Documentation for generated types.

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

// Has is True if bits & state is non 0.
func (s State) Has(bits State) bool { return s&bits != 0 }

func (s State) LocalUninit() bool  { return s.Has(SLocalUninit) }
func (s State) LocalActive() bool  { return s.Has(SLocalActive) }
func (s State) LocalClosed() bool  { return s.Has(SLocalClosed) }
func (s State) RemoteUninit() bool { return s.Has(SRemoteUninit) }
func (s State) RemoteActive() bool { return s.Has(SRemoteActive) }
func (s State) RemoteClosed() bool { return s.Has(SRemoteClosed) }

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
	// Human readable name
	String() string
}

// CloseError sets an error condition on an endpoint and closes the endpoint.
func CloseError(e Endpoint, err error) {
	e.Condition().SetError(err)
	e.Close()
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

// Context information per connection.
type connectionContext struct {
	injecter Injecter
	str      string
}

var connectionContexts = internal.MakeSafeMap()

// Injecter for event-loop associated with this connection.
func (c Connection) Injecter() Injecter {
	if cc, ok := connectionContexts.Get(c).(connectionContext); ok {
		return cc.injecter
	}
	return nil
}

// Unique (per process) string identifier for a connection, useful for debugging.
func (c Connection) String() string {
	if cc, ok := connectionContexts.Get(c).(connectionContext); ok {
		return cc.str
	}
	return fmt.Sprintf("%x", c.pn)
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

func (s Session) String() string {
	return fmt.Sprintf("%s/%p", s.Connection(), s.pn)
}

func (l Link) Connection() Connection { return l.Session().Connection() }

// Human-readable link description including name, source, target and direction.
func (l Link) String() string {
	if l.IsSender() {
		return fmt.Sprintf("%s(%s->%s)", l.Name(), l.Source().Address(), l.Target().Address())
	} else {
		return fmt.Sprintf("%s(%s<-%s)", l.Name(), l.Target().Address(), l.Source().Address())
	}
}

// Error returns an instance of amqp.Error or nil.
func (c Condition) Error() error {
	if c.IsNil() || !c.IsSet() {
		return nil
	}
	return amqp.Error{c.Name(), c.Description()}
}

// Set a Go error into a condition.
// If it is not an amqp.Condition use the error type as name, error string as description.
func (c Condition) SetError(err error) {
	if err != nil {
		if cond, ok := err.(amqp.Error); ok {
			c.SetName(cond.Name)
			c.SetDescription(cond.Description)
		} else {
			c.SetName(reflect.TypeOf(err).Name())
			c.SetDescription(err.Error())
		}
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
func pnTime(t time.Time) C.pn_timestamp_t {
	secs := t.Unix()
	// Note: sub-second accuracy is not guaraunteed if the Unix time in
	// nanoseconds cannot be represented by an int64 (sometime around year 2260)
	msecs := (t.UnixNano() % int64(time.Second)) / int64(time.Millisecond)
	return C.pn_timestamp_t(secs*1000 + msecs)
}

// goTime converts a pn_timestamp_t to a Go time.Time.
func goTime(t C.pn_timestamp_t) time.Time {
	secs := int64(t) / 1000
	nsecs := (int64(t) % 1000) * int64(time.Millisecond)
	return time.Unix(secs, nsecs)
}

// Special treatment for Transport.Head, return value is unsafe.Pointer not string
func (t Transport) Head() unsafe.Pointer {
	return unsafe.Pointer(C.pn_transport_head(t.pn))
}

// Special treatment for Transport.Push, takes []byte instead of char*, size
func (t Transport) Push(bytes []byte) int {
	return int(C.pn_transport_push(t.pn, (*C.char)(unsafe.Pointer(&bytes[0])), C.size_t(len(bytes))))
}
