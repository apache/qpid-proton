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

// CoreHandler handles core proton events.
type CoreHandler interface {
	// Handle is called with an event.
	// Typically Handle() is implemented as a switch on e.Type()
	Handle(e Event) error
}

// cHandler wraps a C pn_handler_t
type cHandler struct {
	pn *C.pn_handler_t
}

func (h cHandler) Handle(e Event) error {
	C.pn_handler_dispatch(h.pn, e.pn, C.pn_event_type(e.pn))
	return nil // FIXME aconway 2015-03-31: error handling
}

func HandShaker() CoreHandler {
	return cHandler{C.pn_handshaker()}
}

func FlowController(prefetch int) CoreHandler {
	return cHandler{C.pn_flowcontroller(C.int(prefetch))}
}

// MessagingHandler provides a higher-level, easier-to-use interface for writing
// applications that send and receive messages.
//
// You can implement this interface and wrap it with a MessagingHandlerDelegator
type MessagingHandler interface {
	Handle(MessagingEventType, Event) error
}

// MessagingEventType provides an easier set of event types to work with
// that the core proton EventType.
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

	// The connection is closed.
	MConnectionClosed

	// The session is closed.
	MSessionClosed

	// The link is closed.
	MLinkClosed

	// The socket is disconnected.
	MDisconnected

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

	// A message is received. Call proton.EventMessage(Event) to get the message.
	// To manage the outcome of this messages (e.g. to accept or reject the message)
	// use Event.Delivery().
	MMessage
)

// Capture common patterns for endpoints opening/closing
type endpointDelegator struct {
	remoteOpen, remoteClose, localOpen, localClose EventType
	opening, opened, closing, closed, error        MessagingEventType
	endpoint                                       func(Event) Endpoint
	delegate                                       MessagingHandler
}

func (d endpointDelegator) Handle(e Event) error {
	endpoint := d.endpoint(e)
	state := endpoint.State()

	switch e.Type() {

	case d.localOpen:
		if state.RemoteOpen() {
			return d.delegate.Handle(d.opened, e)
		}

	case d.remoteOpen:
		switch {
		case state.LocalOpen():
			return d.delegate.Handle(d.opened, e)
		case state.LocalUninitialized():
			err := d.delegate.Handle(d.opening, e)
			if err == nil {
				endpoint.Open()
			}
			return err
		}

	case d.remoteClose:
		switch {
		case endpoint.RemoteCondition().IsSet():
			d.delegate.Handle(d.error, e)
		case state.LocalClosed():
			d.delegate.Handle(d.closed, e)
		default:
			d.delegate.Handle(d.closing, e)
		}
		endpoint.Close()

	case d.localClose:
		// Nothing to do

	default:
		panic("internal error") // We shouldn't be called with any other event type.
	}
	return nil
}

// MessagingDelegator implments a CoreHandler and delegates to a MessagingHandler.
// You can modify the exported fields before you pass the MessagingDelegator to
// a Pump
type MessagingDelegator struct {
	delegate                   MessagingHandler
	connection, session, link  endpointDelegator
	handshaker, flowcontroller CoreHandler

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

func NewMessagingDelegator(h MessagingHandler) CoreHandler {
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
		handshaker:     HandShaker(),
		flowcontroller: nil,
		AutoSettle:     true,
		AutoAccept:     true,
		Prefetch:       10,
		PeerCloseError: false,
	}
}

func handleIf(h CoreHandler, e Event) error {
	if h != nil {
		return h.Handle(e)
	}
	return nil
}

func (d *MessagingDelegator) Handle(e Event) error {
	handleIf(d.handshaker, e)
	handleIf(d.flowcontroller, e) // FIXME aconway 2015-03-31: error handling.

	switch e.Type() {

	case EConnectionInit:
		d.flowcontroller = FlowController(d.Prefetch)
		d.delegate.Handle(MStart, e)

	case EConnectionRemoteOpen, EConnectionRemoteClose, EConnectionLocalOpen, EConnectionLocalClose:
		return d.connection.Handle(e)

	case ESessionRemoteOpen, ESessionRemoteClose, ESessionLocalOpen, ESessionLocalClose:
		return d.session.Handle(e)

	case ELinkRemoteOpen, ELinkRemoteClose, ELinkLocalOpen, ELinkLocalClose:
		return d.link.Handle(e)

	case ELinkFlow:
		if e.Link().IsSender() && e.Link().Credit() > 0 {
			return d.delegate.Handle(MSendable, e)
		}

	case EDelivery:
		if e.Delivery().Link().IsReceiver() {
			d.incoming(e)
		} else {
			d.outgoing(e)
		}
	}
	return nil
}

func (d *MessagingDelegator) incoming(e Event) (err error) {
	delivery := e.Delivery()
	if delivery.Readable() && !delivery.Partial() {
		if e.Link().State().LocalClosed() {
			e.Link().Advance()
			if d.AutoAccept {
				delivery.Release(false)
			}
		} else {
			err = d.delegate.Handle(MMessage, e)
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
		err = d.delegate.Handle(MSettled, e)
	}
	return
}

func (d *MessagingDelegator) outgoing(e Event) (err error) {
	delivery := e.Delivery()
	if delivery.Updated() {
		switch delivery.Remote().Type() {
		case Accepted:
			err = d.delegate.Handle(MAccepted, e)
		case Rejected:
			err = d.delegate.Handle(MRejected, e)
		case Released, Modified:
			err = d.delegate.Handle(MReleased, e)
		}
		if err == nil && delivery.Settled() {
			err = d.delegate.Handle(MSettled, e)
		}
		if err == nil && d.AutoSettle {
			delivery.Settle()
		}
	}
	return
}
