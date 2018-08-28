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

import collections
import time
import threading

from cproton import pn_reactor_collector, pn_collector_release

from ._exceptions import ProtonException, ConnectionException, LinkException, Timeout
from ._delivery import Delivery
from ._endpoints import Endpoint, Link
from ._events import Handler
from ._url import Url

from ._reactor import Container
from ._handlers import MessagingHandler, IncomingMessageHandler


class BlockingLink(object):
    def __init__(self, connection, link):
        self.connection = connection
        self.link = link
        self.connection.wait(lambda: not (self.link.state & Endpoint.REMOTE_UNINIT),
                             msg="Opening link %s" % link.name)
        self._checkClosed()

    def _waitForClose(self, timeout=1):
        try:
            self.connection.wait(lambda: self.link.state & Endpoint.REMOTE_CLOSED,
                                 timeout=timeout,
                                 msg="Opening link %s" % self.link.name)
        except Timeout as e:
            pass
        self._checkClosed()

    def _checkClosed(self):
        if self.link.state & Endpoint.REMOTE_CLOSED:
            self.link.close()
            if not self.connection.closing:
                raise LinkDetached(self.link)

    def close(self):
        self.link.close()
        self.connection.wait(lambda: not (self.link.state & Endpoint.REMOTE_ACTIVE),
                             msg="Closing link %s" % self.link.name)

    # Access to other link attributes.
    def __getattr__(self, name):
        return getattr(self.link, name)


class SendException(ProtonException):
    """
    Exception used to indicate an exceptional state/condition on a send request
    """

    def __init__(self, state):
        self.state = state


def _is_settled(delivery):
    return delivery.settled or delivery.link.snd_settle_mode == Link.SND_SETTLED


class BlockingSender(BlockingLink):
    def __init__(self, connection, sender):
        super(BlockingSender, self).__init__(connection, sender)
        if self.link.target and self.link.target.address and self.link.target.address != self.link.remote_target.address:
            # this may be followed by a detach, which may contain an error condition, so wait a little...
            self._waitForClose()
            # ...but close ourselves if peer does not
            self.link.close()
            raise LinkException("Failed to open sender %s, target does not match" % self.link.name)

    def send(self, msg, timeout=False, error_states=None):
        delivery = self.link.send(msg)
        self.connection.wait(lambda: _is_settled(delivery), msg="Sending on sender %s" % self.link.name,
                             timeout=timeout)
        if delivery.link.snd_settle_mode != Link.SND_SETTLED:
            delivery.settle()
        bad = error_states
        if bad is None:
            bad = [Delivery.REJECTED, Delivery.RELEASED]
        if delivery.remote_state in bad:
            raise SendException(delivery.remote_state)
        return delivery


class Fetcher(MessagingHandler):
    def __init__(self, connection, prefetch):
        super(Fetcher, self).__init__(prefetch=prefetch, auto_accept=False)
        self.connection = connection
        self.incoming = collections.deque([])
        self.unsettled = collections.deque([])

    def on_message(self, event):
        self.incoming.append((event.message, event.delivery))
        self.connection.container.yield_()  # Wake up the wait() loop to handle the message.

    def on_link_error(self, event):
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            event.link.close()
            if not self.connection.closing:
                raise LinkDetached(event.link)

    def on_connection_error(self, event):
        if not self.connection.closing:
            raise ConnectionClosed(event.connection)

    @property
    def has_message(self):
        return len(self.incoming)

    def pop(self):
        message, delivery = self.incoming.popleft()
        if not delivery.settled:
            self.unsettled.append(delivery)
        return message

    def settle(self, state=None):
        delivery = self.unsettled.popleft()
        if state:
            delivery.update(state)
        delivery.settle()


class BlockingReceiver(BlockingLink):
    def __init__(self, connection, receiver, fetcher, credit=1):
        super(BlockingReceiver, self).__init__(connection, receiver)
        if self.link.source and self.link.source.address and self.link.source.address != self.link.remote_source.address:
            # this may be followed by a detach, which may contain an error condition, so wait a little...
            self._waitForClose()
            # ...but close ourselves if peer does not
            self.link.close()
            raise LinkException("Failed to open receiver %s, source does not match" % self.link.name)
        if credit: receiver.flow(credit)
        self.fetcher = fetcher
        self.container = connection.container

    def __del__(self):
        self.fetcher = None
        # The next line causes a core dump if the Proton-C reactor finalizes
        # first.  The self.container reference prevents out of order reactor
        # finalization. It may not be set if exception in BlockingLink.__init__
        if hasattr(self, "container"):
            self.link.handler = None  # implicit call to reactor

    def receive(self, timeout=False):
        if not self.fetcher:
            raise Exception("Can't call receive on this receiver as a handler was provided")
        if not self.link.credit:
            self.link.flow(1)
        self.connection.wait(lambda: self.fetcher.has_message, msg="Receiving on receiver %s" % self.link.name,
                             timeout=timeout)
        return self.fetcher.pop()

    def accept(self):
        self.settle(Delivery.ACCEPTED)

    def reject(self):
        self.settle(Delivery.REJECTED)

    def release(self, delivered=True):
        if delivered:
            self.settle(Delivery.MODIFIED)
        else:
            self.settle(Delivery.RELEASED)

    def settle(self, state=None):
        if not self.fetcher:
            raise Exception("Can't call accept/reject etc on this receiver as a handler was provided")
        self.fetcher.settle(state)


class LinkDetached(LinkException):
    def __init__(self, link):
        self.link = link
        if link.is_sender:
            txt = "sender %s to %s closed" % (link.name, link.target.address)
        else:
            txt = "receiver %s from %s closed" % (link.name, link.source.address)
        if link.remote_condition:
            txt += " due to: %s" % link.remote_condition
            self.condition = link.remote_condition.name
        else:
            txt += " by peer"
            self.condition = None
        super(LinkDetached, self).__init__(txt)


class ConnectionClosed(ConnectionException):
    def __init__(self, connection):
        self.connection = connection
        txt = "Connection %s closed" % connection.hostname
        if connection.remote_condition:
            txt += " due to: %s" % connection.remote_condition
            self.condition = connection.remote_condition.name
        else:
            txt += " by peer"
            self.condition = None
        super(ConnectionClosed, self).__init__(txt)


class BlockingConnection(Handler):
    """
    A synchronous style connection wrapper.

    This object's implementation uses OS resources.  To ensure they
    are released when the object is no longer in use, make sure that
    object operations are enclosed in a try block and that close() is
    always executed on exit.
    """

    def __init__(self, url, timeout=None, container=None, ssl_domain=None, heartbeat=None, **kwargs):
        self.disconnected = False
        self.timeout = timeout or 60
        self.container = container or Container()
        self.container.timeout = self.timeout
        self.container.start()
        self.url = Url(url).defaults()
        self.conn = None
        self.closing = False
        failed = True
        try:
            self.conn = self.container.connect(url=self.url, handler=self, ssl_domain=ssl_domain, reconnect=False,
                                               heartbeat=heartbeat, **kwargs)
            self.wait(lambda: not (self.conn.state & Endpoint.REMOTE_UNINIT),
                      msg="Opening connection")
            failed = False
        finally:
            if failed and self.conn:
                self.close()

    def create_sender(self, address, handler=None, name=None, options=None):
        return BlockingSender(self, self.container.create_sender(self.conn, address, name=name, handler=handler,
                                                                 options=options))

    def create_receiver(self, address, credit=None, dynamic=False, handler=None, name=None, options=None):
        prefetch = credit
        if handler:
            fetcher = None
            if prefetch is None:
                prefetch = 1
        else:
            fetcher = Fetcher(self, credit)
        return BlockingReceiver(
            self,
            self.container.create_receiver(self.conn, address, name=name, dynamic=dynamic, handler=handler or fetcher,
                                           options=options), fetcher, credit=prefetch)

    def close(self):
        # TODO: provide stronger interrupt protection on cleanup.  See PEP 419
        if self.closing:
            return
        self.closing = True
        self.container.errors = []
        try:
            if self.conn:
                self.conn.close()
                self.wait(lambda: not (self.conn.state & Endpoint.REMOTE_ACTIVE),
                          msg="Closing connection")
        finally:
            self.conn.free()
            # Nothing left to block on.  Allow reactor to clean up.
            self.run()
            self.conn = None
            self.container.global_handler = None  # break circular ref: container to cadapter.on_error
            pn_collector_release(pn_reactor_collector(self.container._impl))  # straggling event may keep reactor alive
            self.container = None

    def _is_closed(self):
        return self.conn.state & (Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED)

    def run(self):
        """ Hand control over to the event loop (e.g. if waiting indefinitely for incoming messages) """
        while self.container.process(): pass
        self.container.stop()
        self.container.process()

    def wait(self, condition, timeout=False, msg=None):
        """Call process until condition() is true"""
        if timeout is False:
            timeout = self.timeout
        if timeout is None:
            while not condition() and not self.disconnected:
                self.container.process()
        else:
            container_timeout = self.container.timeout
            self.container.timeout = timeout
            try:
                deadline = time.time() + timeout
                while not condition() and not self.disconnected:
                    self.container.process()
                    if deadline < time.time():
                        txt = "Connection %s timed out" % self.url
                        if msg: txt += ": " + msg
                        raise Timeout(txt)
            finally:
                self.container.timeout = container_timeout
        if self.disconnected or self._is_closed():
            self.container.stop()
            self.conn.handler = None  # break cyclical reference
        if self.disconnected and not self._is_closed():
            raise ConnectionException(
                "Connection %s disconnected: %s" % (self.url, self.disconnected))

    def on_link_remote_close(self, event):
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            event.link.close()
            if not self.closing:
                raise LinkDetached(event.link)

    def on_connection_remote_close(self, event):
        if event.connection.state & Endpoint.LOCAL_ACTIVE:
            event.connection.close()
            if not self.closing:
                raise ConnectionClosed(event.connection)

    def on_transport_tail_closed(self, event):
        self.on_transport_closed(event)

    def on_transport_head_closed(self, event):
        self.on_transport_closed(event)

    def on_transport_closed(self, event):
        self.disconnected = event.transport.condition or "unknown"


class AtomicCount(object):
    def __init__(self, start=0, step=1):
        """Thread-safe atomic counter. Start at start, increment by step."""
        self.count, self.step = start, step
        self.lock = threading.Lock()

    def next(self):
        """Get the next value"""
        self.lock.acquire()
        self.count += self.step;
        result = self.count
        self.lock.release()
        return result


class SyncRequestResponse(IncomingMessageHandler):
    """
    Implementation of the synchronous request-response (aka RPC) pattern.
    @ivar address: Address for all requests, may be None.
    @ivar connection: Connection for requests and responses.
    """

    correlation_id = AtomicCount()

    def __init__(self, connection, address=None):
        """
        Send requests and receive responses. A single instance can send many requests
        to the same or different addresses.

        @param connection: A L{BlockingConnection}
        @param address: Address for all requests.
            If not specified, each request must have the address property set.
            Successive messages may have different addresses.
        """
        super(SyncRequestResponse, self).__init__()
        self.connection = connection
        self.address = address
        self.sender = self.connection.create_sender(self.address)
        # dynamic=true generates a unique address dynamically for this receiver.
        # credit=1 because we want to receive 1 response message initially.
        self.receiver = self.connection.create_receiver(None, dynamic=True, credit=1, handler=self)
        self.response = None

    def call(self, request):
        """
        Send a request message, wait for and return the response message.

        @param request: A L{proton.Message}. If L{self.address} is not set the 
            L{self.address} must be set and will be used.
        """
        if not self.address and not request.address:
            raise ValueError("Request message has no address: %s" % request)
        request.reply_to = self.reply_to
        request.correlation_id = correlation_id = str(self.correlation_id.next())
        self.sender.send(request)

        def wakeup():
            return self.response and (self.response.correlation_id == correlation_id)

        self.connection.wait(wakeup, msg="Waiting for response")
        response = self.response
        self.response = None  # Ready for next response.
        self.receiver.flow(1)  # Set up credit for the next response.
        return response

    @property
    def reply_to(self):
        """Return the dynamic address of our receiver."""
        return self.receiver.remote_source.address

    def on_message(self, event):
        """Called when we receive a message for our receiver."""
        self.response = event.message
        self.connection.container.yield_()  # Wake up the wait() loop to handle the message.
