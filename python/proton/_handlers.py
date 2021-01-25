#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from __future__ import absolute_import

import errno
import logging
import socket
import time
import weakref

from ._condition import Condition
from ._delivery import Delivery
from ._endpoints import Endpoint
from ._events import Event, Handler, _dispatch
from ._exceptions import ProtonException
from ._io import IO
from ._message import Message
from ._selectable import Selectable
from ._transport import Transport
from ._url import Url

log = logging.getLogger("proton")


class OutgoingMessageHandler(Handler):
    """
    A utility for simpler and more intuitive handling of delivery
    events related to outgoing i.e. sent messages.

    :param auto_settle: If ``True``, settle all messages (default). Otherwise
        messages must be explicitly settled.
    :type auto_settle: ``bool``
    :param delegate: A client handler for the endpoint event
    """

    def __init__(self, auto_settle=True, delegate=None):
        self.auto_settle = auto_settle
        self.delegate = delegate

    def on_link_flow(self, event):
        if event.link.is_sender and event.link.credit \
                and event.link.state & Endpoint.LOCAL_ACTIVE \
                and event.link.state & Endpoint.REMOTE_ACTIVE:
            self.on_sendable(event)

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.link.is_sender and dlv.updated:
            if dlv.remote_state == Delivery.ACCEPTED:
                self.on_accepted(event)
            elif dlv.remote_state == Delivery.REJECTED:
                self.on_rejected(event)
            elif dlv.remote_state == Delivery.RELEASED or dlv.remote_state == Delivery.MODIFIED:
                self.on_released(event)
            if dlv.settled:
                self.on_settled(event)
                if self.auto_settle:
                    dlv.settle()

    def on_sendable(self, event):
        """
        Called when the sender link has credit and messages can
        therefore be transferred.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_sendable', event)

    def on_accepted(self, event):
        """
        Called when the remote peer accepts an outgoing message.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_accepted', event)

    def on_rejected(self, event):
        """
        Called when the remote peer rejects an outgoing message.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_rejected', event)

    def on_released(self, event):
        """
        Called when the remote peer releases an outgoing message. Note
        that this may be in response to either the ``RELEASE`` or ``MODIFIED``
        state as defined by the AMQP specification.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_released', event)

    def on_settled(self, event):
        """
        Called when the remote peer has settled the outgoing
        message. This is the point at which it should never be
        retransmitted.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_settled', event)


def recv_msg(delivery):
    msg = Message()
    msg.decode(delivery.link.recv(delivery.pending))
    delivery.link.advance()
    return msg


class Reject(ProtonException):
    """
    An exception that indicates a message should be rejected.
    """
    pass


class Release(ProtonException):
    """
    An exception that indicates a message should be released.
    """
    pass


class Acking(object):
    """
    A class containing methods for handling received messages.
    """

    def accept(self, delivery):
        """
        Accepts a received message.

        .. note:: This method cannot currently be used in combination
            with transactions. See :class:`proton.reactor.Transaction`
            for transactional methods.

        :param delivery: The message delivery tracking object
        :type delivery: :class:`proton.Delivery`
        """
        self.settle(delivery, Delivery.ACCEPTED)

    def reject(self, delivery):
        """
        Rejects a received message that is considered invalid or
        unprocessable.

        .. note:: This method cannot currently be used in combination
            with transactions. See :class:`proton.reactor.Transaction`
            for transactional methods.

        :param delivery: The message delivery tracking object
        :type delivery: :class:`proton.Delivery`
        """
        self.settle(delivery, Delivery.REJECTED)

    def release(self, delivery, delivered=True):
        """
        Releases a received message, making it available at the source
        for any (other) interested receiver. The ``delivered``
        parameter indicates whether this should be considered a
        delivery attempt (and the delivery count updated) or not.

        .. note:: This method cannot currently be used in combination
            with transactions. See :class:`proton.reactor.Transaction`
            for transactional methods.

        :param delivery: The message delivery tracking object
        :type delivery: :class:`proton.Delivery`
        :param delivered: If ``True``, the message will be annotated
            with a delivery attempt (setting delivery flag
            :const:`proton.Delivery.MODIFIED`). Otherwise, the message
            will be returned without the annotation and released (setting
            delivery flag :const:`proton.Delivery.RELEASED`
        :type delivered: ``bool``
        """
        if delivered:
            self.settle(delivery, Delivery.MODIFIED)
        else:
            self.settle(delivery, Delivery.RELEASED)

    def settle(self, delivery, state=None):
        """
        Settles the message delivery, and optionally updating the
        delivery state.

        :param delivery: The message delivery tracking object
        :type delivery: :class:`proton.Delivery`
        :param state: The delivery state, or ``None`` if not update
            is to be performed.
        :type  state: ``int`` or ``None``
        """
        if state:
            delivery.update(state)
        delivery.settle()


class IncomingMessageHandler(Handler, Acking):
    """
    A utility for simpler and more intuitive handling of delivery
    events related to incoming i.e. received messages.

    :type auto_accept: ``bool``
    :param auto_accept: If ``True``, accept all messages (default). Otherwise
        messages must be individually accepted or rejected.
    :param delegate: A client handler for the endpoint event
    """

    def __init__(self, auto_accept=True, delegate=None):
        self.delegate = delegate
        self.auto_accept = auto_accept

    def on_delivery(self, event):
        dlv = event.delivery
        if not dlv.link.is_receiver:
            return
        if dlv.aborted:
            self.on_aborted(event)
            dlv.settle()
        elif dlv.readable and not dlv.partial:
            event.message = recv_msg(dlv)
            if event.link.state & Endpoint.LOCAL_CLOSED:
                if self.auto_accept:
                    dlv.update(Delivery.RELEASED)
                    dlv.settle()
            else:
                try:
                    self.on_message(event)
                    if self.auto_accept:
                        dlv.update(Delivery.ACCEPTED)
                        dlv.settle()
                except Reject:
                    dlv.update(Delivery.REJECTED)
                    dlv.settle()
                except Release:
                    dlv.update(Delivery.MODIFIED)
                    dlv.settle()
        elif dlv.updated and dlv.settled:
            self.on_settled(event)

    def on_message(self, event):
        """
        Called when a message is received. The message itself can be
        obtained as a property on the event. For the purpose of
        referring to this message in further actions (e.g. if
        explicitly accepting it, the ``delivery`` should be used, also
        obtainable via a property on the event.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_message', event)

    def on_settled(self, event):
        """
        Callback for when a message delivery is settled by the remote peer.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_settled', event)

    def on_aborted(self, event):
        """
        Callback for when a message delivery is aborted by the remote peer.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_aborted', event)


class EndpointStateHandler(Handler):
    """
    A utility that exposes 'endpoint' events - ie the open/close for
    links, sessions and connections in a more intuitive manner. A
    ``XXX_opened()`` method will be called when both local and remote peers
    have opened the link, session or connection. This can be used to
    confirm a locally initiated action for example. A ``XXX_opening()``
    method will be called when the remote peer has requested an open
    that was not initiated locally. By default this will simply open
    locally, which then triggers the ``XXX_opened()`` call. The same applies
    to close.

    :param peer_close_is_error: If ``True``, a peer endpoint closing will be
        treated as an error with an error callback. Otherwise (default), the
        normal callbacks for the closing will occur.
    :type peer_close_is_error:  ``bool``
    :param delegate: A client handler for the endpoint event
    """

    def __init__(self, peer_close_is_error=False, delegate=None):
        self.delegate = delegate
        self.peer_close_is_error = peer_close_is_error

    @classmethod
    def is_local_open(cls, endpoint):
        """
        Test if local ``enpoint`` is open (ie has state
        :const:`proton.Endpoint.LOCAL_ACTIVE`).

        :param endpoint: The local endpoint to be tested.
        :type endpoint: Any child of :class:`proton.Endpoint`
        :return: ``True`` if local endpoint is in state
            :const:`proton.Endpoint.LOCAL_ACTIVE`, ``False`` otherwise.
        :rtype: ``bool``
        """
        return endpoint.state & Endpoint.LOCAL_ACTIVE

    @classmethod
    def is_local_uninitialised(cls, endpoint):
        """
        Test if local ``enpoint`` is uninitialised (ie has state
        :const:`proton.Endpoint.LOCAL_UNINIT`).

        :param endpoint: The local endpoint to be tested.
        :type endpoint: Any child of :class:`proton.Endpoint`
        :return: ``True`` if local endpoint is in state
            :const:`proton.Endpoint.LOCAL_UNINIT`, ``False`` otherwise.
        :rtype: ``bool``
        """
        return endpoint.state & Endpoint.LOCAL_UNINIT

    @classmethod
    def is_local_closed(cls, endpoint):
        """
        Test if local ``enpoint`` is closed (ie has state
        :const:`proton.Endpoint.LOCAL_CLOSED`).

        :param endpoint: The local endpoint to be tested.
        :type endpoint: Any child of :class:`proton.Endpoint`
        :return: ``True`` if local endpoint is in state
            :const:`proton.Endpoint.LOCAL_CLOSED`, ``False`` otherwise.
        :rtype: ``bool``
        """
        return endpoint.state & Endpoint.LOCAL_CLOSED

    @classmethod
    def is_remote_open(cls, endpoint):
        """
        Test if remote ``enpoint`` is open (ie has state
        :const:`proton.Endpoint.LOCAL_ACTIVE`).

        :param endpoint: The remote endpoint to be tested.
        :type endpoint: Any child of :class:`proton.Endpoint`
        :return: ``True`` if remote endpoint is in state
            :const:`proton.Endpoint.LOCAL_ACTIVE`, ``False`` otherwise.
        :rtype: ``bool``
        """
        return endpoint.state & Endpoint.REMOTE_ACTIVE

    @classmethod
    def is_remote_closed(cls, endpoint):
        """
        Test if remote ``enpoint`` is closed (ie has state
        :const:`proton.Endpoint.REMOTE_CLOSED`).

        :param endpoint: The remote endpoint to be tested.
        :type endpoint: Any child of :class:`proton.Endpoint`
        :return: ``True`` if remote endpoint is in state
            :const:`proton.Endpoint.REMOTE_CLOSED`, ``False`` otherwise.
        :rtype: ``bool``
        """
        return endpoint.state & Endpoint.REMOTE_CLOSED

    @classmethod
    def print_error(cls, endpoint, endpoint_type):
        """
        Logs an error message related to an error condition at an endpoint.

        :param endpoint: The endpoint to be tested
        :type endpoint: :class:`proton.Endpoint`
        :param endpoint_type: The endpoint type as a string to be printed
            in the log message.
        :type endpoint_type: ``str``
        """
        if endpoint.remote_condition:
            log.error(endpoint.remote_condition.description or endpoint.remote_condition.name)
        elif cls.is_local_open(endpoint) and cls.is_remote_closed(endpoint):
            log.error("%s closed by peer" % endpoint_type)

    def on_link_remote_close(self, event):
        if event.link.remote_condition:
            self.on_link_error(event)
        elif self.is_local_closed(event.link):
            self.on_link_closed(event)
        else:
            self.on_link_closing(event)
        event.link.close()

    def on_session_remote_close(self, event):
        if event.session.remote_condition:
            self.on_session_error(event)
        elif self.is_local_closed(event.session):
            self.on_session_closed(event)
        else:
            self.on_session_closing(event)
        event.session.close()

    def on_connection_remote_close(self, event):
        if event.connection.remote_condition:
            if event.connection.remote_condition.name == "amqp:connection:forced":
                # Treat this the same as just having the transport closed by the peer without
                # sending any events. Allow reconnection to happen transparently.
                return
            self.on_connection_error(event)
        elif self.is_local_closed(event.connection):
            self.on_connection_closed(event)
        else:
            self.on_connection_closing(event)
        event.connection.close()

    def on_connection_local_open(self, event):
        if self.is_remote_open(event.connection):
            self.on_connection_opened(event)

    def on_connection_remote_open(self, event):
        if self.is_local_open(event.connection):
            self.on_connection_opened(event)
        elif self.is_local_uninitialised(event.connection):
            self.on_connection_opening(event)
            event.connection.open()

    def on_session_local_open(self, event):
        if self.is_remote_open(event.session):
            self.on_session_opened(event)

    def on_session_remote_open(self, event):
        if self.is_local_open(event.session):
            self.on_session_opened(event)
        elif self.is_local_uninitialised(event.session):
            self.on_session_opening(event)
            event.session.open()

    def on_link_local_open(self, event):
        if self.is_remote_open(event.link):
            self.on_link_opened(event)

    def on_link_remote_open(self, event):
        if self.is_local_open(event.link):
            self.on_link_opened(event)
        elif self.is_local_uninitialised(event.link):
            self.on_link_opening(event)
            event.link.open()

    def on_connection_opened(self, event):
        """
        Callback for when both the local and remote endpoints of a
        connection have opened.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_opened', event)

    def on_session_opened(self, event):
        """
        Callback for when both the local and remote endpoints of a
        session have opened.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_opened', event)

    def on_link_opened(self, event):
        """
        Callback for when both the local and remote endpoints of a
        link have opened.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_opened', event)

    def on_connection_opening(self, event):
        """
        Callback for when a remote peer initiates the opening of
        a connection.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_opening', event)

    def on_session_opening(self, event):
        """
        Callback for when a remote peer initiates the opening of
        a session.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_opening', event)

    def on_link_opening(self, event):
        """
        Callback for when a remote peer initiates the opening of
        a link.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_opening', event)

    def on_connection_error(self, event):
        """
        Callback for when an initiated connection open fails.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_error', event)
        else:
            self.print_error(event.connection, "connection")

    def on_session_error(self, event):
        """
        Callback for when an initiated session open fails.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_error', event)
        else:
            self.print_error(event.session, "session")
            event.connection.close()

    def on_link_error(self, event):
        """
        Callback for when an initiated link open fails.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_error', event)
        else:
            self.print_error(event.link, "link")
            event.connection.close()

    def on_connection_closed(self, event):
        """
        Callback for when both the local and remote endpoints of a
        connection have closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_closed', event)

    def on_session_closed(self, event):
        """
        Callback for when both the local and remote endpoints of a
        session have closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_closed', event)

    def on_link_closed(self, event):
        """
        Callback for when both the local and remote endpoints of a
        link have closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_closed', event)

    def on_connection_closing(self, event):
        """
        Callback for when a remote peer initiates the closing of
        a connection.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_closing', event)
        elif self.peer_close_is_error:
            self.on_connection_error(event)

    def on_session_closing(self, event):
        """
        Callback for when a remote peer initiates the closing of
        a session.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_closing', event)
        elif self.peer_close_is_error:
            self.on_session_error(event)

    def on_link_closing(self, event):
        """
        Callback for when a remote peer initiates the closing of
        a link.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_closing', event)
        elif self.peer_close_is_error:
            self.on_link_error(event)

    def on_transport_tail_closed(self, event):
        """
        Callback for when the transport tail has closed (ie no further input will
        be accepted by the transport).

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        self.on_transport_closed(event)

    def on_transport_closed(self, event):
        """
        Callback for when the transport has closed - ie both the head (input) and
        tail (output) of the transport pipeline are closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if self.delegate is not None and event.connection and self.is_local_open(event.connection):
            _dispatch(self.delegate, 'on_disconnected', event)


class MessagingHandler(Handler, Acking):
    """
    A general purpose handler that makes the proton-c events somewhat
    simpler to deal with and/or avoids repetitive tasks for common use
    cases.

    :param prefetch: Initial flow credit for receiving messages, defaults to 10.
    :type prefetch: ``int``
    :param auto_accept: If ``True``, accept all messages (default). Otherwise
        messages must be individually accepted or rejected.
    :type auto_accept: ``bool``
    :param auto_settle: If ``True``, settle all messages (default). Otherwise
        messages must be explicitly settled.
    :type auto_settle: ``bool``
    :param peer_close_is_error: If ``True``, a peer endpoint closing will be
        treated as an error with an error callback. Otherwise (default), the
        normal callbacks for the closing will occur.
    :type peer_close_is_error:  ``bool``
    """

    def __init__(self, prefetch=10, auto_accept=True, auto_settle=True, peer_close_is_error=False):
        self.handlers = []
        if prefetch:
            self.handlers.append(FlowController(prefetch))
        self.handlers.append(EndpointStateHandler(peer_close_is_error, weakref.proxy(self)))
        self.handlers.append(IncomingMessageHandler(auto_accept, weakref.proxy(self)))
        self.handlers.append(OutgoingMessageHandler(auto_settle, weakref.proxy(self)))
        self.fatal_conditions = ["amqp:unauthorized-access"]

    def on_transport_error(self, event):
        """
        Called when some error is encountered with the transport over
        which the AMQP connection is to be established. This includes
        authentication errors as well as socket errors.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if event.transport.condition:
            if event.transport.condition.info:
                log.error("%s: %s: %s" % (
                    event.transport.condition.name, event.transport.condition.description,
                    event.transport.condition.info))
            else:
                log.error("%s: %s" % (event.transport.condition.name, event.transport.condition.description))
            if event.transport.condition.name in self.fatal_conditions:
                event.connection.close()
        else:
            logging.error("Unspecified transport error")

    def on_connection_error(self, event):
        """
        Called when the peer closes the connection with an error condition.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        EndpointStateHandler.print_error(event.connection, "connection")

    def on_session_error(self, event):
        """
        Called when the peer closes the session with an error condition.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        EndpointStateHandler.print_error(event.session, "session")
        event.connection.close()

    def on_link_error(self, event):
        """
        Called when the peer closes the link with an error condition.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        EndpointStateHandler.print_error(event.link, "link")
        event.connection.close()

    def on_reactor_init(self, event):
        """
        Called when the event loop - the reactor - starts.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        if hasattr(event.reactor, 'subclass'):
            setattr(event, event.reactor.subclass.__name__.lower(), event.reactor)
        self.on_start(event)

    def on_start(self, event):
        """
        Called when the event loop starts. (Just an alias for on_reactor_init)

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_connection_closed(self, event):
        """
        Called when the connection is closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_session_closed(self, event):
        """
        Called when the session is closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_link_closed(self, event):
        """
        Called when the link is closed.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_connection_closing(self, event):
        """
        Called when the peer initiates the closing of the connection.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_session_closing(self, event):
        """
        Called when the peer initiates the closing of the session.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_link_closing(self, event):
        """
        Called when the peer initiates the closing of the link.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_disconnected(self, event):
        """
        Called when the socket is disconnected.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_sendable(self, event):
        """
        Called when the sender link has credit and messages can
        therefore be transferred.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_accepted(self, event):
        """
        Called when the remote peer accepts an outgoing message.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_rejected(self, event):
        """
        Called when the remote peer rejects an outgoing message.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_released(self, event):
        """
        Called when the remote peer releases an outgoing message. Note
        that this may be in response to either the RELEASE or MODIFIED
        state as defined by the AMQP specification.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_settled(self, event):
        """
        Called when the remote peer has settled the outgoing
        message. This is the point at which it should never be
        retransmitted.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_message(self, event):
        """
        Called when a message is received. The message itself can be
        obtained as a property on the event. For the purpose of
        referring to this message in further actions (e.g. if
        explicitly accepting it, the ``delivery`` should be used, also
        obtainable via a property on the event.

        :param event: The underlying event object. Use this to obtain further
            information on the event. In particular, the message itself may
            be obtained by accessing ``event.message``.
        :type event: :class:`proton.Event`
        """
        pass


class TransactionHandler(object):
    """
    The interface for transaction handlers - ie objects that want to
    be notified of state changes related to a transaction.
    """

    def on_transaction_declared(self, event):
        """
        Called when a local transaction is declared.

        :param event: The underlying event object. Use this to obtain further
            information on the event. In particular, the :class:`proton.reactor.Transaction`
            object may be obtained by accessing ``event.transaction``.
        :type event: :class:`proton.Event`
        """
        pass

    def on_transaction_committed(self, event):
        """
        Called when a local transaction is discharged successfully
        (committed).

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_transaction_aborted(self, event):
        """
        Called when a local transaction is discharged unsuccessfully
        (aborted).

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_transaction_declare_failed(self, event):
        """
        Called when a local transaction declare fails.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass

    def on_transaction_commit_failed(self, event):
        """
        Called when the commit of a local transaction fails.

        :param event: The underlying event object. Use this to obtain further
            information on the event.
        :type event: :class:`proton.Event`
        """
        pass


class TransactionalClientHandler(MessagingHandler, TransactionHandler):
    """
    An extension to the MessagingHandler for applications using
    transactions. This handler provides all of the callbacks found
    in :class:`MessagingHandler` and :class:`TransactionHandler`,
    and provides a convenience method :meth:`accept` for performing
    a transactional acceptance of received messages.

    :param prefetch: Initial flow credit for receiving messages, defaults to 10.
    :type prefetch: ``int``
    :param auto_accept: If ``True``, accept all messages (default). Otherwise messages
        must be individually accepted or rejected.
    :type auto_accept: ``bool``
    :param auto_settle: If ``True``, settle all messages (default). Otherwise
        messages must be explicitly settled.
    :type auto_settle: ``bool``
    :param peer_close_is_error: If ``True``, a peer endpoint closing will be
        treated as an error with an error callback. Otherwise (default), the
        normal callbacks for the closing will occur.
    :type peer_close_is_error:  ``bool``
    """

    def __init__(self, prefetch=10, auto_accept=False, auto_settle=True, peer_close_is_error=False):
        super(TransactionalClientHandler, self).__init__(prefetch, auto_accept, auto_settle, peer_close_is_error)

    def accept(self, delivery, transaction=None):
        """
        A convenience method for accepting a received message as part of a
        transaction. If no transaction object is supplied, a regular
        non-transactional acceptance will be performed.

        :param delivery: Delivery tracking object for received message.
        :type delivery: :class:`proton.Delivery`
        :param transaction: Transaction tracking object which is required if
            the message is being accepted under the transaction. If ``None`` (default),
            then a normal non-transactional accept occurs.
        :type transaction: :class:`proton.reactor.Transaction`
        """
        if transaction:
            transaction.accept(delivery)
        else:
            super(TransactionalClientHandler, self).accept(delivery)


class FlowController(Handler):
    def __init__(self, window=1024):
        self._window = window
        self._drained = 0

    def on_link_local_open(self, event):
        self._flow(event.link)

    def on_link_remote_open(self, event):
        self._flow(event.link)

    def on_link_flow(self, event):
        self._flow(event.link)

    def on_delivery(self, event):
        self._flow(event.link)

    def _flow(self, link):
        if link.is_receiver:
            self._drained += link.drained()
            if self._drained == 0:
                delta = self._window - link.credit
                link.flow(delta)


class Handshaker(Handler):

    @staticmethod
    def on_connection_remote_open(event):
        conn = event.connection
        if conn.state & Endpoint.LOCAL_UNINIT:
            conn.open()

    @staticmethod
    def on_session_remote_open(event):
        ssn = event.session
        if ssn.state & Endpoint.LOCAL_UNINIT:
            ssn.open()

    @staticmethod
    def on_link_remote_open(event):
        link = event.link
        if link.state & Endpoint.LOCAL_UNINIT:
            link.source.copy(link.remote_source)
            link.target.copy(link.remote_target)
            link.open()

    @staticmethod
    def on_connection_remote_close(event):
        conn = event.connection
        if not conn.state & Endpoint.LOCAL_CLOSED:
            conn.close()

    @staticmethod
    def on_session_remote_close(event):
        ssn = event.session
        if not ssn.state & Endpoint.LOCAL_CLOSED:
            ssn.close()

    @staticmethod
    def on_link_remote_close(event):
        link = event.link
        if not link.state & Endpoint.LOCAL_CLOSED:
            link.close()


# Back compatibility definitions
CFlowController = FlowController
CHandshaker = Handshaker


class PythonIO:

    def __init__(self):
        self.selectables = []
        self.delegate = IOHandler()

    def on_unhandled(self, method, event):
        event.dispatch(self.delegate)

    def on_selectable_init(self, event):
        self.selectables.append(event.context)

    def on_selectable_updated(self, event):
        pass

    def on_selectable_final(self, event):
        sel = event.context
        if sel.is_terminal:
            self.selectables.remove(sel)
            sel.close()

    def on_reactor_quiesced(self, event):
        reactor = event.reactor
        # check if we are still quiesced, other handlers of
        # on_reactor_quiesced could have produced events to process
        if not reactor.quiesced:
            return

        reading = []
        writing = []
        deadline = None
        for sel in self.selectables:
            if sel.reading:
                reading.append(sel)
            if sel.writing:
                writing.append(sel)
            if sel.deadline:
                if deadline is None:
                    deadline = sel.deadline
                else:
                    deadline = min(sel.deadline, deadline)

        if deadline is not None:
            timeout = deadline - time.time()
        else:
            timeout = reactor.timeout
        if timeout < 0:
            timeout = 0
        timeout = min(timeout, reactor.timeout)
        readable, writable, _ = IO.select(reading, writing, [], timeout)

        now = reactor.mark()

        for s in readable:
            s.readable()
        for s in writable:
            s.writable()
        for s in self.selectables:
            if s.deadline and now > s.deadline:
                s.expired()

        reactor.yield_()


# For C style IO handler need to implement Selector
class IOHandler(Handler):

    def __init__(self):
        self._selector = IO.Selector()

    def on_selectable_init(self, event):
        s = event.selectable
        self._selector.add(s)
        s._reactor._selectables += 1

    def on_selectable_updated(self, event):
        s = event.selectable
        self._selector.update(s)

    def on_selectable_final(self, event):
        s = event.selectable
        self._selector.remove(s)
        s._reactor._selectables -= 1
        s.close()

    def on_reactor_quiesced(self, event):
        r = event.reactor

        if not r.quiesced:
            return

        d = r.timer_deadline
        readable, writable, expired = self._selector.select(r.timeout)

        now = r.mark()

        for s in readable:
            s.readable()
        for s in writable:
            s.writable()
        for s in expired:
            s.expired()

        r.yield_()

    def on_selectable_readable(self, event):
        s = event.selectable
        t = s._transport

        # If we're an acceptor we can't have a transport
        # and we don't want to do anything here in any case
        if not t:
            return

        capacity = t.capacity()
        if capacity > 0:
            try:
                b = s.recv(capacity)
                if len(b) > 0:
                    n = t.push(b)
                else:
                    # EOF handling
                    self.on_selectable_error(event)
            except socket.error as e:
                # TODO: What's the error handling to be here?
                log.error("Couldn't recv: %r" % e)
                t.close_tail()

        # Always update as we may have gone to not reading or from
        # not writing to writing when processing the incoming bytes
        r = s._reactor
        self.update(t, s, r.now)

    def on_selectable_writable(self, event):
        s = event.selectable
        t = s._transport

        # If we're an acceptor we can't have a transport
        # and we don't want to do anything here in any case
        if not t:
            return

        pending = t.pending()
        if pending > 0:

            try:
                n = s.send(t.peek(pending))
                t.pop(n)
            except socket.error as e:
                log.error("Couldn't send: %r" % e)
                # TODO: Error? or actually an exception
                t.close_head()

        newpending = t.pending()
        if newpending != pending:
            r = s._reactor
            self.update(t, s, r.now)

    def on_selectable_error(self, event):
        s = event.selectable
        t = s._transport

        t.close_head()
        t.close_tail()
        s.terminate()
        s._transport = None
        t._selectable = None
        s.update()

    def on_selectable_expired(self, event):
        s = event.selectable
        t = s._transport
        r = s._reactor

        self.update(t, s, r.now)

    def on_connection_local_open(self, event):
        c = event.connection
        if not c.state & Endpoint.REMOTE_UNINIT:
            return

        t = Transport()
        # It seems perverse, but the C code ignores bind errors too!
        # and this is required or you get errors because Connector() has already
        # bound the transport and connection!
        t.bind_nothrow(c)

    def on_connection_bound(self, event):
        c = event.connection
        t = event.transport

        reactor = c._reactor

        # link the new transport to its reactor:
        t._reactor = reactor

        if c._acceptor:
            # this connection was created by the acceptor.  There is already a
            # socket assigned to this connection.  Nothing needs to be done.
            return

        url = c.url or Url(c.hostname)
        url.defaults()

        host = url.host
        port = int(url.port)

        if not c.user:
            user = url.username
            if user:
                c.user = user
            password = url.password
            if password:
                c.password = password

        addrs = socket.getaddrinfo(host, port, socket.AF_UNSPEC, socket.SOCK_STREAM)

        # Try first possible address
        log.debug("Connect trying first transport address: %s", addrs[0])
        sock = IO.connect(addrs[0])

        # At this point we need to arrange to be called back when the socket is writable
        ConnectSelectable(sock, reactor, addrs[1:], t, self)

        # TODO: Don't understand why we need this now - how can we get PN_TRANSPORT until the connection succeeds?
        t._selectable = None

    @staticmethod
    def update(transport, selectable, now):
        try:
            capacity = transport.capacity()
            selectable.reading = capacity > 0
        except:
            if transport.closed:
                selectable.terminate()
                selectable._transport = None
                transport._selectable = None
        try:
            pending = transport.pending()
            selectable.writing = pending > 0
        except:
            if transport.closed:
                selectable.terminate()
                selectable._transport = None
                transport._selectable = None
        selectable.deadline = transport.tick(now)
        selectable.update()

    def on_transport(self, event):
        t = event.transport
        r = t._reactor
        s = t._selectable
        if s and not s.is_terminal:
            self.update(t, s, r.now)

    def on_transport_closed(self, event):
        t = event.transport
        r = t._reactor
        s = t._selectable
        if s and not s.is_terminal:
            s.terminate()
            s._transport = None
            t._selectable = None
            r.update(s)
        t.unbind()


class ConnectSelectable(Selectable):
    def __init__(self, sock, reactor, addrs, transport, iohandler):
        super(ConnectSelectable, self).__init__(sock, reactor)
        self.writing = True
        self._addrs = addrs
        self._transport = transport
        self._iohandler = iohandler
        transport._connect_selectable = self

    def readable(self):
        pass

    def writable(self):
        e = self._delegate.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
        t = self._transport
        t._connect_selectable = None

        # Always cleanup this ConnectSelectable: either we failed or created a new one
        # Do it first to ensure the socket gets deregistered before being registered again
        # in the case of connecting
        self.terminate()
        self._transport = None
        self.update()

        if e == 0:
            log.debug("Connection succeeded")

            # Disassociate from the socket (which will be passed on)
            self.release()

            s = self._reactor.selectable(delegate=self._delegate)
            s._transport = t
            t._selectable = s
            self._iohandler.update(t, s, t._reactor.now)

            return
        elif e == errno.ECONNREFUSED:
            if len(self._addrs) > 0:
                log.debug("Connection refused: trying next transport address: %s", self._addrs[0])

                sock = IO.connect(self._addrs[0])
                # New ConnectSelectable for the new socket with rest of addresses
                ConnectSelectable(sock, self._reactor, self._addrs[1:], t, self._iohandler)
                return
            else:
                log.debug("Connection refused, but tried all transport addresses")
                t.condition = Condition("proton.pythonio", "Connection refused to all addresses")
        else:
            log.error("Couldn't connect: %s", e)
            t.condition = Condition("proton.pythonio", "Connection error: %s" % e)

        t.close_tail()
        t.close_head()
