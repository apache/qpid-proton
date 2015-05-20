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

// #include <proton/reactor.h>
// #include <proton/handlers.h>
import "C"

import (
	"qpid.apache.org/proton/go/internal"
)

// EventHandler handles core proton events.
type EventHandler interface {
	// HandleEvent is called with an event.
	// Typically HandleEvent() is implemented as a switch on e.Type()
	HandleEvent(e Event) error
}

// cHandler wraps a C pn_handler_t
type cHandler struct {
	pn *C.pn_handler_t
}

func (h cHandler) HandleEvent(e Event) error {
	C.pn_handler_dispatch(h.pn, e.pn, C.pn_event_type(e.pn))
	return nil // FIXME aconway 2015-03-31: error handling
}

// MessagingHandler provides an alternative interface to EventHandler.
// it is easier to use for most applications that send and receive messages.
//
// Implement this interface and then wrap your value with a MessagingHandlerDelegator.
// MessagingHandlerDelegator implements EventHandler and can be registered with a Pump.
//
type MessagingHandler interface {
	HandleMessagingEvent(MessagingEventType, Event) error
}

// MessagingEventType provides a set of events that are easier to work with than the
// core events defined by EventType
//
// There are 3 types of "endpoint": Connection, Session and Link.
// For each endpoint there are 5 event types: Opening, Opened, Closing, Closed and Error.
// The meaning of these events is as follows:
//
// Opening: The remote end opened, the local end will open automatically.
//
// Opened: Both ends are open, regardless of which end opened first.
//
// Closing: The remote end closed without error, the local end will close automatically.
//
// Error: The remote end closed with an error, the local end will close automatically.
//
// Closed: Both ends are closed, regardless of which end closed first or if there was an error.
//
type MessagingEventType int

const (
	// The event loop starts.
	MStart MessagingEventType = iota
	// The peer closes the connection with an error condition.
	MConnectionError
	// The peer closes the session with an error condition.
	MSessionError
	// The peer closes the link with an error condition.
	MLinkError
	// The peer Initiates the opening of the connection.
	MConnectionOpening
	// The peer initiates the opening of the session.
	MSessionOpening
	// The peer initiates the opening of the link.
	MLinkOpening
	// The connection is opened.
	MConnectionOpened
	// The session is opened.
	MSessionOpened
	// The link is opened.
	MLinkOpened
	// The peer initiates the closing of the connection.
	MConnectionClosing
	// The peer initiates the closing of the session.
	MSessionClosing
	// The peer initiates the closing of the link.
	MLinkClosing
	// Both ends of the connection are closed.
	MConnectionClosed
	// Both ends of the session are closed.
	MSessionClosed
	// Both ends of the link are closed.
	MLinkClosed
	// The connection is disconnected.
	MConnectionDisconnected
	// The session's connection was disconnected
	MSessionDisconnected
	// The session's connection was disconnected
	MLinkDisconnected
	// The sender link has credit and messages can
	// therefore be transferred.
	MSendable
	// The remote peer accepts an outgoing message.
	MAccepted
	// The remote peer rejects an outgoing message.
	MRejected
	// The peer releases an outgoing message. Note that this may be in response to
	// either the RELEASE or MODIFIED state as defined by the AMQP specification.
	MReleased
	// The peer has settled the outgoing message. This is the point at which it
	// shouod never be retransmitted.
	MSettled
	// A message is received. Call DecodeMessage() to decode as an amqp.Message.
	// To manage the outcome of this messages (e.g. to accept or reject the message)
	// use Event.Delivery().
	MMessage
	// The event loop terminates, there are no more events to process.
	MFinal
)

func (t MessagingEventType) String() string {
	switch t {
	case MStart:
		return "Start"
	case MConnectionError:
		return "ConnectionError"
	case MSessionError:
		return "SessionError"
	case MLinkError:
		return "LinkError"
	case MConnectionOpening:
		return "ConnectionOpening"
	case MSessionOpening:
		return "SessionOpening"
	case MLinkOpening:
		return "LinkOpening"
	case MConnectionOpened:
		return "ConnectionOpened"
	case MSessionOpened:
		return "SessionOpened"
	case MLinkOpened:
		return "LinkOpened"
	case MConnectionClosing:
		return "ConnectionClosing"
	case MSessionClosing:
		return "SessionClosing"
	case MLinkClosing:
		return "LinkClosing"
	case MConnectionClosed:
		return "ConnectionClosed"
	case MSessionClosed:
		return "SessionClosed"
	case MLinkClosed:
		return "LinkClosed"
	case MConnectionDisconnected:
		return "ConnectionDisconnected"
	case MSessionDisconnected:
		return "MSessionDisconnected"
	case MLinkDisconnected:
		return "MLinkDisconnected"
	case MSendable:
		return "Sendable"
	case MAccepted:
		return "Accepted"
	case MRejected:
		return "Rejected"
	case MReleased:
		return "Released"
	case MSettled:
		return "Settled"
	case MMessage:
		return "Message"
	default:
		return "Unknown"
	}
}

// ResourceHandler provides a simple way to track the creation and deletion of
// various proton objects.
// endpointDelegator captures common patterns for endpoints opening/closing
type endpointDelegator struct {
	remoteOpen, remoteClose, localOpen, localClose EventType
	opening, opened, closing, closed, error        MessagingEventType
	endpoint                                       func(Event) Endpoint
	delegate                                       MessagingHandler
}

// HandleEvent handles an open/close event for an endpoint in a generic way.
func (d endpointDelegator) HandleEvent(e Event) (err error) {
	endpoint := d.endpoint(e)
	state := endpoint.State()

	switch e.Type() {

	case d.localOpen:
		if state.Is(SRemoteActive) {
			err = d.delegate.HandleMessagingEvent(d.opened, e)
		}

	case d.remoteOpen:
		switch {
		case state.Is(SLocalActive):
			err = d.delegate.HandleMessagingEvent(d.opened, e)
		case state.Is(SLocalUninit):
			err = d.delegate.HandleMessagingEvent(d.opening, e)
			if err == nil {
				endpoint.Open()
			}
		}

	case d.remoteClose:
		var err1 error
		if endpoint.RemoteCondition().IsSet() { // Closed with error
			err1 = d.delegate.HandleMessagingEvent(d.error, e)
			if err1 == nil { // Don't overwrite an application error.
				err1 = endpoint.RemoteCondition().Error()
			}
		} else {
			err1 = d.delegate.HandleMessagingEvent(d.closing, e)
		}
		if state.Is(SLocalClosed) {
			err = d.delegate.HandleMessagingEvent(d.closed, e)
		} else if state.Is(SLocalActive) {
			endpoint.Close()
		}
		if err1 != nil { // Keep the first error.
			err = err1
		}

	case d.localClose:
		if state.Is(SRemoteClosed) {
			err = d.delegate.HandleMessagingEvent(d.closed, e)
		}

	default:
		// We shouldn't be called with any other event type.
		panic(internal.Errorf("internal error, not an open/close event: %s", e))
	}

	return err
}

// MessagingDelegator implments a EventHandler and delegates to a MessagingHandler.
// You can modify the exported fields before you pass the MessagingDelegator to
// a Pump.
type MessagingDelegator struct {
	delegate                   MessagingHandler
	connection, session, link  endpointDelegator
	handshaker, flowcontroller EventHandler

	// AutoSettle (default true) automatically pre-settle outgoing messages.
	AutoSettle bool
	// AutoAccept (default true) automatically accept and settle incoming messages
	// if they are not settled by the delegate.
	AutoAccept bool
	// Prefetch (default 10) initial credit to issue for incoming links.
	Prefetch int
	// PeerCloseIsError (default false) if true a close by the peer will be treated as an error.
	PeerCloseError bool
}

func NewMessagingDelegator(h MessagingHandler) EventHandler {
	return &MessagingDelegator{
		delegate: h,
		connection: endpointDelegator{
			EConnectionRemoteOpen, EConnectionRemoteClose, EConnectionLocalOpen, EConnectionLocalClose,
			MConnectionOpening, MConnectionOpened, MConnectionClosing, MConnectionClosed,
			MConnectionError,
			func(e Event) Endpoint { return e.Connection() },
			h,
		},
		session: endpointDelegator{
			ESessionRemoteOpen, ESessionRemoteClose, ESessionLocalOpen, ESessionLocalClose,
			MSessionOpening, MSessionOpened, MSessionClosing, MSessionClosed,
			MSessionError,
			func(e Event) Endpoint { return e.Session() },
			h,
		},
		link: endpointDelegator{
			ELinkRemoteOpen, ELinkRemoteClose, ELinkLocalOpen, ELinkLocalClose,
			MLinkOpening, MLinkOpened, MLinkClosing, MLinkClosed,
			MLinkError,
			func(e Event) Endpoint { return e.Link() },
			h,
		},
		flowcontroller: nil,
		AutoSettle:     true,
		AutoAccept:     true,
		Prefetch:       10,
		PeerCloseError: false,
	}
}

func handleIf(h EventHandler, e Event) error {
	if h != nil {
		return h.HandleEvent(e)
	}
	return nil
}

// Handle a proton event by passing the corresponding MessagingEvent(s) to
// the MessagingHandler.
func (d *MessagingDelegator) HandleEvent(e Event) error {
	handleIf(d.flowcontroller, e) // FIXME aconway 2015-03-31: error handling.

	switch e.Type() {

	case EConnectionInit:
		d.flowcontroller = cHandler{C.pn_flowcontroller(C.int(d.Prefetch))}
		d.delegate.HandleMessagingEvent(MStart, e)

	case EConnectionRemoteOpen, EConnectionRemoteClose, EConnectionLocalOpen, EConnectionLocalClose:
		return d.connection.HandleEvent(e)

	case ESessionRemoteOpen, ESessionRemoteClose, ESessionLocalOpen, ESessionLocalClose:
		return d.session.HandleEvent(e)

	case ELinkRemoteOpen, ELinkRemoteClose, ELinkLocalOpen, ELinkLocalClose:
		return d.link.HandleEvent(e)

	case ELinkFlow:
		if e.Link().IsSender() && e.Link().Credit() > 0 {
			return d.delegate.HandleMessagingEvent(MSendable, e)
		}

	case EDelivery:
		if e.Delivery().Link().IsReceiver() {
			d.incoming(e)
		} else {
			d.outgoing(e)
		}

	case ETransportTailClosed:
		c := e.Connection()
		for l := c.LinkHead(SRemoteActive); !l.IsNil(); l = l.Next(SRemoteActive) {
			e2 := e
			e2.link = l
			e2.session = l.Session()
			d.delegate.HandleMessagingEvent(MLinkDisconnected, e2)
		}
		for s := c.SessionHead(SRemoteActive); !s.IsNil(); s = s.Next(SRemoteActive) {
			e2 := e
			e2.session = s
			d.delegate.HandleMessagingEvent(MSessionDisconnected, e2)
		}
		d.delegate.HandleMessagingEvent(MConnectionDisconnected, e)
		d.delegate.HandleMessagingEvent(MFinal, e)
	}
	return nil
}

func (d *MessagingDelegator) incoming(e Event) (err error) {
	delivery := e.Delivery()
	if delivery.Readable() && !delivery.Partial() {
		if e.Link().State().Is(SLocalClosed) {
			e.Link().Advance()
			if d.AutoAccept {
				delivery.Release(false)
			}
		} else {
			err = d.delegate.HandleMessagingEvent(MMessage, e)
			e.Link().Advance()
			if d.AutoAccept && !delivery.Settled() {
				if err == nil {
					delivery.Accept()
				} else {
					delivery.Reject()
				}
			}
		}
	} else if delivery.Updated() && delivery.Settled() {
		err = d.delegate.HandleMessagingEvent(MSettled, e)
	}
	return
}

func (d *MessagingDelegator) outgoing(e Event) (err error) {
	delivery := e.Delivery()
	if delivery.Updated() {
		switch delivery.Remote().Type() {
		case Accepted:
			err = d.delegate.HandleMessagingEvent(MAccepted, e)
		case Rejected:
			err = d.delegate.HandleMessagingEvent(MRejected, e)
		case Released, Modified:
			err = d.delegate.HandleMessagingEvent(MReleased, e)
		}
		if err == nil && delivery.Settled() {
			err = d.delegate.HandleMessagingEvent(MSettled, e)
		}
		if err == nil && d.AutoSettle {
			delivery.Settle()
		}
	}
	return
}
