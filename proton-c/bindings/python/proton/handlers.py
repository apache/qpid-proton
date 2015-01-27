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
import heapq, logging, os, Queue, re, socket, time, types
from proton import dispatch, generate_uuid, PN_ACCEPTED, SASL, symbol, ulong, Url
from proton import Collector, Connection, Delivery, Described, Endpoint, Event, Link, Terminus, Timeout
from proton import Message, Handler, ProtonException, Transport, TransportException, ConnectionException
from select import select

class FlowController(Handler):
    """
    A handler that controls a configured credit window for associated
    receivers.
    """
    def __init__(self, window=1):
        self.window = window

    def top_up(self, link):
        delta = self.window - link.credit
        link.flow(delta)

    def on_link_local_open(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_link_remote_open(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_link_flow(self, event):
        if event.link.is_receiver:
            self.top_up(event.link)

    def on_delivery(self, event):
        if event.delivery.link.is_receiver:
            self.top_up(event.delivery.link)

def add_nested_handler(handler, nested):
    if hasattr(handler, 'handlers'):
        getattr(handler, 'handlers').append(nested)
    else:
        handler.handlers = [nested]

class ScopedHandler(Handler):
    """
    An internal handler that checks for handlers scoped to the engine
    objects an event relates to. E.g it allows delivery, link, session
    or connection scoped handlers that will only be called with events
    for the object to which they are scoped.
    """
    scopes = ["delivery", "link", "session", "connection"]

    def on_unhandled(self, method, event):
        if event.type in [Event.CONNECTION_FINAL, Event.SESSION_FINAL, Event.LINK_FINAL]:
            return

        objects = [getattr(event, attr) for attr in self.scopes if hasattr(event, attr) and getattr(event, attr)]
        targets = [getattr(o, "context") for o in objects if hasattr(o, "context")]
        for t in targets:
            event.dispatch(t)


class OutgoingMessageHandler(Handler):
    """
    A utility for simpler and more intuitive handling of delivery
    events related to outgoing i.e. sent messages.
    """
    def __init__(self, auto_settle=True, delegate=None):
        self.auto_settle = auto_settle
        self.delegate = delegate

    def on_link_flow(self, event):
        if event.link.is_sender and event.link.credit:
            self.on_sendable(event)

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.link.is_sender and dlv.updated:
            if dlv.remote_state == Delivery.ACCEPTED:
                self.on_accepted(event)
            elif dlv.remote_state == Delivery.REJECTED:
                self.on_rejected(event)
            elif dlv.remote_state == Delivery.RELEASED:
                self.on_released(event)
            elif dlv.remote_state == Delivery.MODIFIED:
                self.on_modified(event)
            if dlv.settled:
                self.on_settled(event)
            if self.auto_settle:
                dlv.settle()

    def on_sendable(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_sendable', event)

    def on_accepted(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_accepted', event)

    def on_rejected(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_rejected', event)

    def on_released(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_released', event)

    def on_modified(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_modified', event)

    def on_settled(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_settled', event)

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

class Acking(object):
    def accept(self, delivery):
        self.settle(delivery, Delivery.ACCEPTED)

    def reject(self, delivery):
        self.settle(delivery, Delivery.REJECTED)

    def release(self, delivery, delivered=True):
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
        if dlv.readable and not dlv.partial:
            event.message = recv_msg(dlv)
            try:
                self.on_message(event)
                if self.auto_accept:
                    dlv.update(Delivery.ACCEPTED)
                    dlv.settle()
            except Reject:
                dlv.update(Delivery.REJECTED)
                dlv.settle()
        elif dlv.updated and dlv.settled:
            self.on_settled(event)

    def on_message(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_message', event)

    def on_settled(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_settled', event)

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
            logging.error(endpoint.remote_condition.description)
        elif cls.is_local_open(endpoint) and cls.is_remote_closed(endpoint):
            logging.error("%s closed by peer" % endpoint_type)

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
        if self.delegate:
            dispatch(self.delegate, 'on_connection_opened', event)

    def on_session_opened(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_opened', event)

    def on_link_opened(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_opened', event)

    def on_connection_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_opening', event)

    def on_session_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_opening', event)

    def on_link_opening(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_opening', event)

    def on_connection_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_error', event)
        else:
            self.log_error(event.connection, "connection")

    def on_session_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_error', event)
        else:
            self.log_error(event.session, "session")
            event.connection.close()

    def on_link_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_error', event)
        else:
            self.log_error(event.link, "link")
            event.connection.close()

    def on_connection_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_closed', event)

    def on_session_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_closed', event)

    def on_link_closed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_closed', event)

    def on_connection_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_connection_closing', event)
        elif self.peer_close_is_error:
            self.on_connection_error(event)

    def on_session_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_closing', event)
        elif self.peer_close_is_error:
            self.on_session_error(event)

    def on_link_closing(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_closing', event)
        elif self.peer_close_is_error:
            self.on_link_error(event)

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
        self.handlers.append(EndpointStateHandler(peer_close_is_error, self))
        self.handlers.append(IncomingMessageHandler(auto_accept, self))
        self.handlers.append(OutgoingMessageHandler(auto_settle, self))

    def on_connection_error(self, event):
        EndpointStateHandler.print_error(event.connection, "connection")

    def on_session_error(self, event):
        EndpointStateHandler.print_error(event.session, "session")
        event.connection.close()

    def on_link_error(self, event):
        EndpointStateHandler.print_error(event.link, "link")
        event.connection.close()

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

from proton import WrappedHandler
from cproton import pn_flowcontroller, pn_handshaker, pn_iohandler

class CFlowController(WrappedHandler):

    def __init__(self, window=1024):
        WrappedHandler.__init__(self, lambda: pn_flowcontroller(window))

class CHandshaker(WrappedHandler):

    def __init__(self):
        WrappedHandler.__init__(self, pn_handshaker)

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
