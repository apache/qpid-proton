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
import heapq, os, Queue, re, socket, time, types
from proton import dispatch, generate_uuid, PN_ACCEPTED, SASL, symbol, ulong, Url
from proton import Collector, Connection, Delivery, Described, Endpoint, Event, Link, Terminus, Timeout
from proton import Message, Handler, ProtonException, Transport, TransportException, ConnectionException
from select import select

class FlowController(Handler):

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
        if not event.delivery.released and event.delivery.link.is_receiver:
            self.top_up(event.delivery.link)

def nested_handlers(handlers):
    # currently only allows for a single level of nesting
    nested = []
    for h in handlers:
        nested.append(h)
        if hasattr(h, 'handlers'):
            nested.extend(getattr(h, 'handlers'))
    return nested

def add_nested_handler(handler, nested):
    if hasattr(handler, 'handlers'):
        getattr(handler, 'handlers').append(nested)
    else:
        handler.handlers = [nested]

class ScopedHandler(Handler):

    scopes = {
        "pn_connection": ["connection"],
        "pn_session": ["session", "connection"],
        "pn_link": ["link", "session", "connection"],
        "pn_delivery": ["delivery", "link", "session", "connection"]
    }

    def on_unhandled(self, method, args):
        event = args[0]
        if event.type in [Event.CONNECTION_FINAL, Event.SESSION_FINAL, Event.LINK_FINAL]:
            return
        objects = [getattr(event, attr) for attr in self.scopes.get(event.clazz, [])]
        targets = [getattr(o, "context") for o in objects if hasattr(o, "context")]
        handlers = [getattr(t, event.type.method) for t in nested_handlers(targets) if hasattr(t, event.type.method)]
        for h in handlers:
            h(event)

class OutgoingMessageHandler(Handler):
    def __init__(self, auto_settle=True, delegate=None):
        self.auto_settle = auto_settle
        self.delegate = delegate

    def on_link_flow(self, event):
        if event.link.is_sender and event.link.credit:
            self.on_credit(event)

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.released: return
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

    def on_credit(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_credit', event)

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
    def __init__(self, auto_accept=True, delegate=None):
        self.delegate = delegate
        self.auto_accept = auto_accept

    def on_delivery(self, event):
        dlv = event.delivery
        if dlv.released or not dlv.link.is_receiver: return
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
    def __init__(self, peer_close_is_error=False, delegate=None):
        self.delegate = delegate
        self.peer_close_is_error = peer_close_is_error

    def is_local_open(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_ACTIVE

    def is_local_uninitialised(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_UNINIT

    def is_local_closed(self, endpoint):
        return endpoint.state & Endpoint.LOCAL_CLOSED

    def is_remote_open(self, endpoint):
        return endpoint.state & Endpoint.REMOTE_ACTIVE

    def is_remote_closed(self, endpoint):
        return endpoint.state & Endpoint.REMOTE_CLOSED

    def print_error(self, endpoint, endpoint_type):
        if endpoint.remote_condition:
            print endpoint.remote_condition.description
        elif self.is_local_open(endpoint) and self.is_remote_closed(endpoint):
            print "%s closed by peer" % endpoint_type

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
            self.print_error(event.connection, "connection")

    def on_session_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_session_error', event)
        else:
            self.print_error(event.session, "session")
            event.connection.close()

    def on_link_error(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_link_error', event)
        else:
            self.print_error(event.link, "link")
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
    def __init__(self, prefetch=10, auto_accept=True, auto_settle=True, peer_close_is_error=False):
        self.handlers = []
        # FlowController if used needs to see event before
        # IncomingMessageHandler, as the latter may involve the
        # delivery being released
        if prefetch:
            self.handlers.append(FlowController(prefetch))
        self.handlers.append(EndpointStateHandler(peer_close_is_error, self))
        self.handlers.append(IncomingMessageHandler(auto_accept, self))
        self.handlers.append(OutgoingMessageHandler(auto_settle, self))

class TransactionalAcking(object):
    def accept(self, delivery, transaction):
        transaction.accept(delivery)

class TransactionHandler(OutgoingMessageHandler, TransactionalAcking):
    def __init__(self, auto_settle=True, delegate=None):
        super(TransactionHandler, self).__init__(auto_settle, delegate)

    def on_settled(self, event):
        if hasattr(event.delivery, "transaction"):
            event.transaction = event.delivery.transaction
            event.delivery.transaction.handle_outcome(event)

    def on_transaction_declared(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_declared', event)

    def on_transaction_committed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_committed', event)

    def on_transaction_aborted(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_aborted', event)

    def on_transaction_declare_failed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_declare_failed', event)

    def on_transaction_commit_failed(self, event):
        if self.delegate:
            dispatch(self.delegate, 'on_transaction_commit_failed', event)

class TransactionalClientHandler(Handler, TransactionalAcking):
    def __init__(self, prefetch=10, auto_accept=False, auto_settle=True, peer_close_is_error=False):
        super(TransactionalClientHandler, self).__init__()
        self.handlers = []
        # FlowController if used needs to see event before
        # IncomingMessageHandler, as the latter may involve the
        # delivery being released
        if prefetch:
            self.handlers.append(FlowController(prefetch))
        self.handlers.append(EndpointStateHandler(peer_close_is_error, self))
        self.handlers.append(IncomingMessageHandler(auto_accept, self))
        self.handlers.append(TransactionHandler(auto_settle, self))
