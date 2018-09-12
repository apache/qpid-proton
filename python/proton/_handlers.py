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

import logging
import time
import weakref
from select import select

from ._delivery import Delivery
from ._endpoints import Endpoint
from ._message import Message
from ._exceptions import ProtonException
from ._events import Handler, _dispatch

log = logging.getLogger("proton")


class OutgoingMessageHandler(Handler):
    """
    A utility for simpler and more intuitive handling of delivery
    events related to outgoing i.e. sent messages.
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
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_sendable', event)

    def on_accepted(self, event):
        """
        Called when the remote peer accepts an outgoing message.
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_accepted', event)

    def on_rejected(self, event):
        """
        Called when the remote peer rejects an outgoing message.
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_rejected', event)

    def on_released(self, event):
        """
        Called when the remote peer releases an outgoing message. Note
        that this may be in response to either the RELEASE or MODIFIED
        state as defined by the AMQP specification.
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_released', event)

    def on_settled(self, event):
        """
        Called when the remote peer has settled the outgoing
        message. This is the point at which it should never be
        retransmitted.
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
    An exception that indicate a message should be rejected
    """
    pass


class Release(ProtonException):
    """
    An exception that indicate a message should be rejected
    """
    pass


class Acking(object):
    def accept(self, delivery):
        """
        Accepts a received message.

        Note that this method cannot currently be used in combination
        with transactions.
        """
        self.settle(delivery, Delivery.ACCEPTED)

    def reject(self, delivery):
        """
        Rejects a received message that is considered invalid or
        unprocessable.
        """
        self.settle(delivery, Delivery.REJECTED)

    def release(self, delivery, delivered=True):
        """
        Releases a received message, making it available at the source
        for any (other) interested receiver. The ``delivered``
        parameter indicates whether this should be considered a
        delivery attempt (and the delivery count updated) or not.
        """
        if delivered:
            self.settle(delivery, Delivery.MODIFIED)
        else:
            self.settle(delivery, Delivery.RELEASED)

    def settle(self, delivery, state=None):
        if state:
            delivery.update(state)
        delivery.settle()


class IncomingMessageHandler(Handler, Acking):
    """
    A utility for simpler and more intuitive handling of delivery
    events related to incoming i.e. received messages.
    """

    def __init__(self, auto_accept=True, delegate=None):
        self.delegate = delegate
        self.auto_accept = auto_accept

    def on_delivery(self, event):
        dlv = event.delivery
        if not dlv.link.is_receiver: return
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
        """
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_message', event)

    def on_settled(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_settled', event)

    def on_aborted(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_aborted', event)


class EndpointStateHandler(Handler):
    """
    A utility that exposes 'endpoint' events i.e. the open/close for
    links, sessions and connections in a more intuitive manner. A
    XXX_opened method will be called when both local and remote peers
    have opened the link, session or connection. This can be used to
    confirm a locally initiated action for example. A XXX_opening
    method will be called when the remote peer has requested an open
    that was not initiated locally. By default this will simply open
    locally, which then triggers the XXX_opened call. The same applies
    to close.
    """

    def __init__(self, peer_close_is_error=False, delegate=None):
        self.delegate = delegate
        self.peer_close_is_error = peer_close_is_error

    @classmethod
    def is_local_open(cls, endpoint):
        return endpoint.state & Endpoint.LOCAL_ACTIVE

    @classmethod
    def is_local_uninitialised(cls, endpoint):
        return endpoint.state & Endpoint.LOCAL_UNINIT

    @classmethod
    def is_local_closed(cls, endpoint):
        return endpoint.state & Endpoint.LOCAL_CLOSED

    @classmethod
    def is_remote_open(cls, endpoint):
        return endpoint.state & Endpoint.REMOTE_ACTIVE

    @classmethod
    def is_remote_closed(cls, endpoint):
        return endpoint.state & Endpoint.REMOTE_CLOSED

    @classmethod
    def print_error(cls, endpoint, endpoint_type):
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
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_opened', event)

    def on_session_opened(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_opened', event)

    def on_link_opened(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_opened', event)

    def on_connection_opening(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_opening', event)

    def on_session_opening(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_opening', event)

    def on_link_opening(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_opening', event)

    def on_connection_error(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_error', event)
        else:
            self.log_error(event.connection, "connection")

    def on_session_error(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_error', event)
        else:
            self.log_error(event.session, "session")
            event.connection.close()

    def on_link_error(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_error', event)
        else:
            self.log_error(event.link, "link")
            event.connection.close()

    def on_connection_closed(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_closed', event)

    def on_session_closed(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_closed', event)

    def on_link_closed(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_closed', event)

    def on_connection_closing(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_connection_closing', event)
        elif self.peer_close_is_error:
            self.on_connection_error(event)

    def on_session_closing(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_session_closing', event)
        elif self.peer_close_is_error:
            self.on_session_error(event)

    def on_link_closing(self, event):
        if self.delegate is not None:
            _dispatch(self.delegate, 'on_link_closing', event)
        elif self.peer_close_is_error:
            self.on_link_error(event)

    def on_transport_tail_closed(self, event):
        self.on_transport_closed(event)

    def on_transport_closed(self, event):
        if self.delegate is not None and event.connection and self.is_local_open(event.connection):
            _dispatch(self.delegate, 'on_disconnected', event)


class MessagingHandler(Handler, Acking):
    """
    A general purpose handler that makes the proton-c events somewhat
    simpler to deal with and/or avoids repetitive tasks for common use
    cases.
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
        """
        EndpointStateHandler.print_error(event.connection, "connection")

    def on_session_error(self, event):
        """
        Called when the peer closes the session with an error condition.
        """
        EndpointStateHandler.print_error(event.session, "session")
        event.connection.close()

    def on_link_error(self, event):
        """
        Called when the peer closes the link with an error condition.
        """
        EndpointStateHandler.print_error(event.link, "link")
        event.connection.close()

    def on_reactor_init(self, event):
        """
        Called when the event loop - the reactor - starts.
        """
        if hasattr(event.reactor, 'subclass'):
            setattr(event, event.reactor.subclass.__name__.lower(), event.reactor)
        self.on_start(event)

    def on_start(self, event):
        """
        Called when the event loop starts. (Just an alias for on_reactor_init)
        """
        pass

    def on_connection_closed(self, event):
        """
        Called when the connection is closed.
        """
        pass

    def on_session_closed(self, event):
        """
        Called when the session is closed.
        """
        pass

    def on_link_closed(self, event):
        """
        Called when the link is closed.
        """
        pass

    def on_connection_closing(self, event):
        """
        Called when the peer initiates the closing of the connection.
        """
        pass

    def on_session_closing(self, event):
        """
        Called when the peer initiates the closing of the session.
        """
        pass

    def on_link_closing(self, event):
        """
        Called when the peer initiates the closing of the link.
        """
        pass

    def on_disconnected(self, event):
        """
        Called when the socket is disconnected.
        """
        pass

    def on_sendable(self, event):
        """
        Called when the sender link has credit and messages can
        therefore be transferred.
        """
        pass

    def on_accepted(self, event):
        """
        Called when the remote peer accepts an outgoing message.
        """
        pass

    def on_rejected(self, event):
        """
        Called when the remote peer rejects an outgoing message.
        """
        pass

    def on_released(self, event):
        """
        Called when the remote peer releases an outgoing message. Note
        that this may be in response to either the RELEASE or MODIFIED
        state as defined by the AMQP specification.
        """
        pass

    def on_settled(self, event):
        """
        Called when the remote peer has settled the outgoing
        message. This is the point at which it should never be
        retransmitted.
        """
        pass

    def on_message(self, event):
        """
        Called when a message is received. The message itself can be
        obtained as a property on the event. For the purpose of
        referring to this message in further actions (e.g. if
        explicitly accepting it, the ``delivery`` should be used, also
        obtainable via a property on the event.
        """
        pass


class TransactionHandler(object):
    """
    The interface for transaction handlers, i.e. objects that want to
    be notified of state changes related to a transaction.
    """

    def on_transaction_declared(self, event):
        pass

    def on_transaction_committed(self, event):
        pass

    def on_transaction_aborted(self, event):
        pass

    def on_transaction_declare_failed(self, event):
        pass

    def on_transaction_commit_failed(self, event):
        pass


class TransactionalClientHandler(MessagingHandler, TransactionHandler):
    """
    An extension to the MessagingHandler for applications using
    transactions.
    """

    def __init__(self, prefetch=10, auto_accept=False, auto_settle=True, peer_close_is_error=False):
        super(TransactionalClientHandler, self).__init__(prefetch, auto_accept, auto_settle, peer_close_is_error)

    def accept(self, delivery, transaction=None):
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
        if ssn.state() & Endpoint.LOCAL_UNINIT:
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


from ._reactor_impl import WrappedHandler
from cproton import pn_iohandler

class IOHandler(WrappedHandler):

    def __init__(self):
        WrappedHandler.__init__(self, pn_iohandler)


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
            sel.release()

    def on_reactor_quiesced(self, event):
        reactor = event.reactor
        # check if we are still quiesced, other handlers of
        # on_reactor_quiesced could have produced events to process
        if not reactor.quiesced: return

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
        if (timeout < 0): timeout = 0
        timeout = min(timeout, reactor.timeout)
        readable, writable, _ = select(reading, writing, [], timeout)

        reactor.mark()

        now = time.time()

        for s in readable:
            s.readable()
        for s in writable:
            s.writable()
        for s in self.selectables:
            if s.deadline and now > s.deadline:
                s.expired()

        reactor.yield_()
