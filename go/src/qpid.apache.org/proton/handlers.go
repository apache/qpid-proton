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

package proton

import "fmt"

// EventHandler handles core proton events.
type EventHandler interface {
	// HandleEvent is called with an event.
	// Typically HandleEvent() is implemented as a switch on e.Type()
	HandleEvent(e Event)
}

// MessagingHandler provides an alternative interface to EventHandler.
// it is easier to use for most applications that send and receive messages.
//
// Implement this interface and then wrap your value with a MessagingHandlerDelegator.
// MessagingHandlerDelegator implements EventHandler and can be registered with a Engine.
//
type MessagingHandler interface {
	// HandleMessagingEvent is called with  MessagingEvent.
	// Typically HandleEvent() is implemented as a switch on e.Type()
	HandleMessagingEvent(MessagingEvent, Event)
}

// MessagingEvent provides a set of events that are easier to work with than the
// core events defined by EventType
//
// There are 3 types of "endpoint": Connection, Session and Link.  For each
// endpoint there are 5 events: Opening, Opened, Closing, Closed and Error.
//
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
// No further events will be received for the endpoint.
//
type MessagingEvent int

const (
	// The event loop starts.
	MStart MessagingEvent = iota
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
	// should never be re-transmitted.
	MSettled
	// A message is received. Call Event.Delivery().Message() to decode as an amqp.Message.
	// To manage the outcome of this messages (e.g. to accept or reject the message)
	// use Event.Delivery().
	MMessage
	// A network connection was disconnected.
	MDisconnected
)

func (t MessagingEvent) String() string {
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
	case MDisconnected:
		return "Disconnected"
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
	opening, opened, closing, closed, error        MessagingEvent
	endpoint                                       func(Event) Endpoint
	delegator                                      *MessagingAdapter
}

// HandleEvent handles an open/close event for an endpoint in a generic way.
func (d endpointDelegator) HandleEvent(e Event) {
	endpoint := d.endpoint(e)
	state := endpoint.State()

	switch e.Type() {

	case d.localOpen:
		if state.RemoteActive() {
			d.delegator.mhandler.HandleMessagingEvent(d.opened, e)
		}

	case d.remoteOpen:
		d.delegator.mhandler.HandleMessagingEvent(d.opening, e)
		switch {
		case state.LocalActive():
			d.delegator.mhandler.HandleMessagingEvent(d.opened, e)
		case state.LocalUninit():
			if d.delegator.AutoOpen {
				endpoint.Open()
			}
		}

	case d.remoteClose:
		if endpoint.RemoteCondition().IsSet() { // Closed with error
			d.delegator.mhandler.HandleMessagingEvent(d.error, e)
		} else {
			d.delegator.mhandler.HandleMessagingEvent(d.closing, e)
		}
		if state.LocalClosed() {
			d.delegator.mhandler.HandleMessagingEvent(d.closed, e)
		} else if state.LocalActive() {
			endpoint.Close()
		}

	case d.localClose:
		if state.RemoteClosed() {
			d.delegator.mhandler.HandleMessagingEvent(d.closed, e)
		}

	default:
		// We shouldn't be called with any other event type.
		panic(fmt.Errorf("internal error, not an open/close event: %s", e))
	}
}

type flowcontroller struct {
	window, drained int
}

func (d flowcontroller) HandleEvent(e Event) {
	link := e.Link()

	switch e.Type() {
	case ELinkLocalOpen, ELinkRemoteOpen, ELinkFlow, EDelivery:
		if link.IsReceiver() {
			d.drained += link.Drained()
			if d.drained != 0 {
				link.Flow(d.window - link.Credit())
			}
		}
	}
}

// MessagingAdapter implements a EventHandler and delegates to a MessagingHandler.
// You can modify the exported fields before you pass the MessagingAdapter to
// a Engine.
type MessagingAdapter struct {
	mhandler                  MessagingHandler
	connection, session, link endpointDelegator
	flowcontroller            EventHandler

	// AutoSettle (default true) automatically pre-settle outgoing messages.
	AutoSettle bool
	// AutoAccept (default true) automatically accept and settle incoming messages
	// if they are not settled by the delegate.
	AutoAccept bool
	// AutoOpen (default true) automatically open remotely opened endpoints.
	AutoOpen bool
	// Prefetch (default 10) initial credit to issue for incoming links.
	Prefetch int
	// PeerCloseIsError (default false) if true a close by the peer will be treated as an error.
	PeerCloseError bool
}

func NewMessagingAdapter(h MessagingHandler) *MessagingAdapter {
	return &MessagingAdapter{
		mhandler:       h,
		flowcontroller: nil,
		AutoSettle:     true,
		AutoAccept:     true,
		AutoOpen:       true,
		Prefetch:       10,
		PeerCloseError: false,
	}
}

func handleIf(h EventHandler, e Event) {
	if h != nil {
		h.HandleEvent(e)
	}
}

// Handle a proton event by passing the corresponding MessagingEvent(s) to
// the MessagingHandler.
func (d *MessagingAdapter) HandleEvent(e Event) {
	handleIf(d.flowcontroller, e)

	switch e.Type() {

	case EConnectionInit:
		d.connection = endpointDelegator{
			EConnectionRemoteOpen, EConnectionRemoteClose, EConnectionLocalOpen, EConnectionLocalClose,
			MConnectionOpening, MConnectionOpened, MConnectionClosing, MConnectionClosed,
			MConnectionError,
			func(e Event) Endpoint { return e.Connection() },
			d,
		}
		d.session = endpointDelegator{
			ESessionRemoteOpen, ESessionRemoteClose, ESessionLocalOpen, ESessionLocalClose,
			MSessionOpening, MSessionOpened, MSessionClosing, MSessionClosed,
			MSessionError,
			func(e Event) Endpoint { return e.Session() },
			d,
		}
		d.link = endpointDelegator{
			ELinkRemoteOpen, ELinkRemoteClose, ELinkLocalOpen, ELinkLocalClose,
			MLinkOpening, MLinkOpened, MLinkClosing, MLinkClosed,
			MLinkError,
			func(e Event) Endpoint { return e.Link() },
			d,
		}
		if d.Prefetch > 0 {
			d.flowcontroller = flowcontroller{window: d.Prefetch, drained: 0}
		}
		d.mhandler.HandleMessagingEvent(MStart, e)

	case EConnectionRemoteOpen:

		d.connection.HandleEvent(e)

	case EConnectionRemoteClose:
		d.connection.HandleEvent(e)
		e.Connection().Transport().CloseTail()

	case EConnectionLocalOpen, EConnectionLocalClose:
		d.connection.HandleEvent(e)

	case ESessionRemoteOpen, ESessionRemoteClose, ESessionLocalOpen, ESessionLocalClose:
		d.session.HandleEvent(e)

	case ELinkRemoteOpen:
		e.Link().Source().Copy(e.Link().RemoteSource())
		e.Link().Target().Copy(e.Link().RemoteTarget())
		d.link.HandleEvent(e)

	case ELinkRemoteClose, ELinkLocalOpen, ELinkLocalClose:
		d.link.HandleEvent(e)

	case ELinkFlow:
		if e.Link().IsSender() && e.Link().Credit() > 0 {
			d.mhandler.HandleMessagingEvent(MSendable, e)
		}

	case EDelivery:
		if e.Delivery().Link().IsReceiver() {
			d.incoming(e)
		} else {
			d.outgoing(e)
		}

	case ETransportTailClosed:
		if !e.Connection().State().RemoteClosed() { // Unexpected transport closed
			e.Transport().CloseHead() // Complete transport close, no connection close expected
		}

	case ETransportClosed:
		d.mhandler.HandleMessagingEvent(MDisconnected, e)
	}
}

func (d *MessagingAdapter) incoming(e Event) {
	delivery := e.Delivery()
	if delivery.HasMessage() {
		d.mhandler.HandleMessagingEvent(MMessage, e)
		if d.AutoAccept && !delivery.Settled() {
			delivery.Accept()
		}
		if delivery.Current() {
			e.Link().Advance()
		}
	} else if delivery.Updated() && delivery.Settled() {
		d.mhandler.HandleMessagingEvent(MSettled, e)
	}
	return
}

func (d *MessagingAdapter) outgoing(e Event) {
	delivery := e.Delivery()
	if delivery.Updated() {
		switch delivery.Remote().Type() {
		case Accepted:
			d.mhandler.HandleMessagingEvent(MAccepted, e)
		case Rejected:
			d.mhandler.HandleMessagingEvent(MRejected, e)
		case Released, Modified:
			d.mhandler.HandleMessagingEvent(MReleased, e)
		}
		if delivery.Settled() {
			// The delivery was settled remotely, inform the local end.
			d.mhandler.HandleMessagingEvent(MSettled, e)
			if d.AutoSettle {
				delivery.Settle()
			}
		}
	}
	return
}
