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

import collections
import time
import threading

from ._exceptions import ProtonException, ConnectionException, LinkException, Timeout
from ._delivery import Delivery
from ._endpoints import Endpoint, Link
from ._events import Handler
from ._url import Url

from ._reactor import Container
from ._handlers import MessagingHandler, IncomingMessageHandler

from typing import Callable, Optional, Union, TYPE_CHECKING, List, Any

try:
    from typing import Literal
except ImportError:
    # https://www.python.org/dev/peps/pep-0560/#class-getitem
    class GenericMeta(type):
        def __getitem__(self, item):
            pass

    class Literal(metaclass=GenericMeta):
        pass

if TYPE_CHECKING:
    from ._delivery import DispositionType
    from ._transport import SSLDomain
    from ._reactor import SenderOption, ReceiverOption, Connection, LinkOption, Backoff
    from ._endpoints import Receiver, Sender
    from ._events import Event
    from ._message import Message


class BlockingLink:
    def __init__(self, connection: 'BlockingConnection', link: Union['Sender', 'Receiver']) -> None:
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

    def _checkClosed(self) -> None:
        if self.link.state & Endpoint.REMOTE_CLOSED:
            self.link.close()
            if not self.connection.closing:
                raise LinkDetached(self.link)

    def close(self):
        """
        Close the link.
        """
        self.link.close()
        self.connection.wait(lambda: not (self.link.state & Endpoint.REMOTE_ACTIVE),
                             msg="Closing link %s" % self.link.name)

    # Access to other link attributes.
    def __getattr__(self, name: str) -> Any:
        return getattr(self.link, name)


class SendException(ProtonException):
    """
    Exception used to indicate an exceptional state/condition on a send request.

    :param state: The delivery state which caused the exception.
    """

    def __init__(self, state: int) -> None:
        self.state = state


def _is_settled(delivery: Delivery) -> bool:
    return delivery.settled or delivery.link.snd_settle_mode == Link.SND_SETTLED


class BlockingSender(BlockingLink):
    """
    A synchronous sender wrapper. This is typically created by calling
    :meth:`BlockingConnection.create_sender`.
    """

    def __init__(self, connection: 'BlockingConnection', sender: 'Sender') -> None:
        super(BlockingSender, self).__init__(connection, sender)
        if self.link.target and self.link.target.address and self.link.target.address != self.link.remote_target.address:
            # this may be followed by a detach, which may contain an error condition, so wait a little...
            self._waitForClose()
            # ...but close ourselves if peer does not
            self.link.close()
            raise LinkException("Failed to open sender %s, target does not match" % self.link.name)

    def send(
            self,
            msg: 'Message',
            timeout: Union[None, Literal[False], float] = False,
            error_states: Optional[List['DispositionType']] = None,
    ) -> Delivery:
        """
        Blocking send which will return only when the send is complete
        and the message settled.

        :param timeout: Timeout in seconds. If ``False``, the value of ``timeout`` used in the
            constructor of the :class:`BlockingConnection` object used in the constructor will be used.
            If ``None``, there is no timeout. Any other value is treated as a timeout in seconds.
        :param error_states: List of delivery flags which when present in Delivery object
            will cause a :class:`SendException` exception to be raised. If ``None``, these
            will default to a list containing :const:`proton.Delivery.REJECTED` and :const:`proton.Delivery.RELEASED`.
        :return: Delivery object for this message.
        """
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
    """
    A message handler for blocking receivers.
    """

    def __init__(self, connection: 'Connection', prefetch: int):
        super(Fetcher, self).__init__(prefetch=prefetch, auto_accept=False)
        self.connection = connection
        self.incoming = collections.deque([])
        self.unsettled = collections.deque([])

    def on_message(self, event: 'Event') -> None:
        self.incoming.append((event.message, event.delivery))
        self.connection.container.yield_()  # Wake up the wait() loop to handle the message.

    def on_link_error(self, event: 'Event') -> None:
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            event.link.close()
            if not self.connection.closing:
                raise LinkDetached(event.link)

    def on_connection_error(self, event: 'Event') -> None:
        if not self.connection.closing:
            raise ConnectionClosed(event.connection)

    @property
    def has_message(self) -> int:
        """
        The number of messages that have been received and are waiting to be
        retrieved with :meth:`pop`.
        """
        return len(self.incoming)

    def pop(self) -> 'Message':
        """
        Get the next available incoming message. If the message is unsettled, its
        delivery object is moved onto the unsettled queue, and can be settled with
        a call to :meth:`settle`.
        """
        message, delivery = self.incoming.popleft()
        if not delivery.settled:
            self.unsettled.append(delivery)
        return message

    def settle(self, state: Optional[int] = None) -> None:
        """
        Settle the next message previously taken with :meth:`pop`.

        :param state:
        :type state:
        """
        delivery = self.unsettled.popleft()
        if state:
            delivery.update(state)
        delivery.settle()


class BlockingReceiver(BlockingLink):
    """
    A synchronous receiver wrapper. This is typically created by calling
    :meth:`BlockingConnection.create_receiver`.
    """

    def __init__(
            self,
            connection: 'BlockingConnection',
            receiver: 'Receiver',
            fetcher: Optional[Fetcher],
            credit: int = 1
    ) -> None:
        super(BlockingReceiver, self).__init__(connection, receiver)
        if self.link.source and self.link.source.address and self.link.source.address != self.link.remote_source.address:
            # this may be followed by a detach, which may contain an error condition, so wait a little...
            self._waitForClose()
            # ...but close ourselves if peer does not
            self.link.close()
            raise LinkException("Failed to open receiver %s, source does not match" % self.link.name)
        if credit:
            receiver.flow(credit)
        self.fetcher = fetcher
        self.container = connection.container

    def __del__(self):
        self.fetcher = None
        # The next line causes a core dump if the Proton-C reactor finalizes
        # first.  The self.container reference prevents out of order reactor
        # finalization. It may not be set if exception in BlockingLink.__init__
        if hasattr(self, "container"):
            self.link.handler = None  # implicit call to reactor

    def receive(
            self,
            timeout: Union[None, Literal[False], float] = False
    ) -> 'Message':
        """
        Blocking receive call which will return only when a message is received or
        a timeout (if supplied) occurs.

        :param timeout: Timeout in seconds. If ``False``, the value of ``timeout`` used in the
            constructor of the :class:`BlockingConnection` object used in the constructor will be used.
            If ``None``, there is no timeout. Any other value is treated as a timeout in seconds.
        """
        if not self.fetcher:
            raise Exception("Can't call receive on this receiver as a handler was not provided")
        if not self.link.credit:
            self.link.flow(1)
        self.connection.wait(lambda: self.fetcher.has_message, msg="Receiving on receiver %s" % self.link.name,
                             timeout=timeout)
        return self.fetcher.pop()

    def accept(self) -> None:
        """
        Accept and settle the received message. The delivery is set to
        :const:`proton.Delivery.ACCEPTED`.
        """
        self.settle(Delivery.ACCEPTED)

    def reject(self) -> None:
        """
        Reject the received message. The delivery is set to
        :const:`proton.Delivery.REJECTED`.
        """
        self.settle(Delivery.REJECTED)

    def release(self, delivered: bool = True) -> None:
        """
        Release the received message.

        :param delivered: If ``True``, the message delivery is being set to
            :const:`proton.Delivery.MODIFIED`, ie being returned to the sender
            and annotated. If ``False``, the message is returned without
            annotations and the delivery set to  :const:`proton.Delivery.RELEASED`.
        """
        if delivered:
            self.settle(Delivery.MODIFIED)
        else:
            self.settle(Delivery.RELEASED)

    def settle(self, state: Optional['DispositionType'] = None):
        """
        Settle any received messages.

        :param state: Update the delivery of all unsettled messages with the
            supplied state, then settle them.
        :type state: ``None`` or a valid delivery state (see
            :class:`proton.Delivery`.
        """
        if not self.fetcher:
            raise Exception("Can't call accept/reject etc on this receiver as a handler was not provided")
        self.fetcher.settle(state)


class LinkDetached(LinkException):
    """
    The exception raised when the remote peer unexpectedly closes a link in a blocking
    context, or an unexpected link error occurs.

    :param link: The link which closed unexpectedly.
    """

    def __init__(self, link: Link) -> None:
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
    """
    The exception raised when the remote peer unexpectedly closes a connection in a blocking
    context, or an unexpected connection error occurs.

    :param connection: The connection which closed unexpectedly.
    """

    def __init__(self, connection: 'Connection') -> None:
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

    :param url: The connection URL.
    :param timeout: Connection timeout in seconds. If ``None``, defaults to 60 seconds.
    :param container: Container to process the events on the connection. If ``None``,
        a new :class:`proton.Container` will be created.
    :param ssl_domain:
    :param heartbeat: A value in seconds indicating the desired frequency of
        heartbeats used to test the underlying socket is alive.
    :param urls: A list of connection URLs to try to connect to.
    :param kwargs: Container keyword arguments. See :class:`proton.reactor.Container`
        for a list of the valid kwargs.
    """

    def __init__(
            self,
            url: Optional[Union[str, Url]] = None,
            timeout: Optional[float] = None,
            container: Optional[Container] = None,
            ssl_domain: Optional['SSLDomain'] = None,
            heartbeat: Optional[float] = None,
            urls: Optional[List[str]] = None,
            reconnect: Union[None, Literal[False], 'Backoff'] = None,
            **kwargs
    ) -> None:
        self.disconnected = False
        self.timeout = timeout or 60
        self.container = container or Container()
        self.container.timeout = self.timeout
        self.container.start()
        self.conn = None
        self.closing = False
        # Preserve previous behaviour if neither reconnect nor urls are supplied
        if url is not None and urls is None and reconnect is None:
            reconnect = False
            url = Url(url).defaults()
        failed = True
        try:
            self.conn = self.container.connect(url=url, handler=self, ssl_domain=ssl_domain, reconnect=reconnect,
                                               heartbeat=heartbeat, urls=urls, **kwargs)
            self.wait(lambda: not (self.conn.state & Endpoint.REMOTE_UNINIT),
                      msg="Opening connection")
            failed = False
        finally:
            if failed and self.conn:
                self.close()

    def create_sender(
            self,
            address: Optional[str],
            handler: Optional[Handler] = None,
            name: Optional[str] = None,
            options: Optional[Union['SenderOption', List['SenderOption'], 'LinkOption', List['LinkOption']]] = None
    ) -> BlockingSender:
        """
        Create a blocking sender.

        :param address: Address of target node.
        :param handler: Event handler for this sender.
        :param name: Sender name.
        :param options: A single option, or a list of sender options
        :return: New blocking sender instance.
        """
        return BlockingSender(self, self.container.create_sender(self.conn, address, name=name, handler=handler,
                                                                 options=options))

    def create_receiver(
            self,
            address: Optional[str] = None,
            credit: Optional[int] = None,
            dynamic: bool = False,
            handler: Optional[Handler] = None,
            name: Optional[str] = None,
            options: Optional[Union['ReceiverOption', List['ReceiverOption'], 'LinkOption', List['LinkOption']]] = None
    ) -> BlockingReceiver:
        """
        Create a blocking receiver.

        :param address: Address of source node.
        :param credit: Initial link flow credit. If not set, will default to 1.
        :param dynamic: If ``True``, indicates dynamic creation of the receiver.
        :param handler: Event handler for this receiver.
        :param name: Receiver name.
        :param options: A single option, or a list of receiver options
        :return: New blocking receiver instance.
        """
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

    def close(self) -> None:
        """
        Close the connection.
        """
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
                if self.conn.transport:
                    # Close tail to force transport cleanup without waiting/hanging for peer close frame.
                    self.conn.transport.close_tail()
        finally:
            self.conn.free()
            # Nothing left to block on.  Allow reactor to clean up.
            self.run()
            if self.conn:
                self.conn.handler = None  # break cyclical reference
                self.conn = None
            self.container.stop_events()
            self.container = None

    @property
    def url(self) -> str:
        """
        The address for this connection.
        """
        return self.conn and self.conn.connected_address

    def _is_closed(self) -> int:
        return self.conn.state & (Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED)

    def run(self) -> None:
        """
        Hand control over to the event loop (e.g. if waiting indefinitely for incoming messages)
        """
        while self.container.process():
            pass
        self.container.stop()
        self.container.process()

    def wait(
            self,
            condition: Callable[[], bool],
            timeout: Union[None, Literal[False], float] = False,
            msg: Optional[str] = None
    ) -> None:
        """
        Process events until ``condition()`` returns ``True``.

        :param condition: Condition which determines when the wait will end.
        :param timeout: Timeout in seconds. If ``False``, the value of ``timeout`` used in the
            constructor of this object will be used. If ``None``, there is no timeout. Any other
            value is treated as a timeout in seconds.
        :param msg: Context message for :class:`proton.Timeout` exception
        """
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
                        if msg:
                            txt += ": " + msg
                        raise Timeout(txt)
            finally:
                self.container.timeout = container_timeout
        if self.disconnected and not self._is_closed():
            raise ConnectionException(
                "Connection %s disconnected: %s" % (self.url, self.disconnected))

    def on_link_remote_close(self, event: 'Event') -> None:
        """
        Event callback for when the remote terminus closes.
        """
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            event.link.close()
            if not self.closing:
                raise LinkDetached(event.link)

    def on_connection_remote_close(self, event: 'Event') -> None:
        """
        Event callback for when the link peer closes the connection.
        """
        if event.connection.state & Endpoint.LOCAL_ACTIVE:
            event.connection.close()
            if not self.closing:
                raise ConnectionClosed(event.connection)

    def on_transport_tail_closed(self, event: 'Event') -> None:
        self.on_transport_closed(event)

    def on_transport_head_closed(self, event: 'Event') -> None:
        self.on_transport_closed(event)

    def on_transport_closed(self, event: 'Event') -> None:
        if not self.closing:
            self.disconnected = event.transport.condition or "unknown"


class AtomicCount:
    def __init__(self, start: int = 0, step: int = 1) -> None:
        """Thread-safe atomic counter. Start at start, increment by step."""
        self.count, self.step = start, step
        self.lock = threading.Lock()

    def next(self) -> int:
        """Get the next value"""
        self.lock.acquire()
        self.count += self.step
        result = self.count
        self.lock.release()
        return result


class SyncRequestResponse(IncomingMessageHandler):
    """
    Implementation of the synchronous request-response (aka RPC) pattern.
    A single instance can send many requests to the same or different
    addresses.

    :param connection: Connection for requests and responses.
    :param address: Address for all requests. If not specified, each request
        must have the address property set. Successive messages may have
        different addresses.
    """

    correlation_id = AtomicCount()

    def __init__(self, connection: BlockingConnection, address: Optional[str] = None) -> None:
        super(SyncRequestResponse, self).__init__()
        self.connection = connection
        self.address = address
        self.sender = self.connection.create_sender(self.address)
        # dynamic=true generates a unique address dynamically for this receiver.
        # credit=1 because we want to receive 1 response message initially.
        self.receiver = self.connection.create_receiver(None, dynamic=True, credit=1, handler=self)
        self.response = None

    def call(self, request: 'Message') -> 'Message':
        """
        Send a request message, wait for and return the response message.

        :param request: Request message. If ``self.address`` is not set the
            request message address must be set and will be used.
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
    def reply_to(self) -> str:
        """
        The dynamic address of our receiver.
        """
        return self.receiver.remote_source.address

    def on_message(self, event: 'Event') -> None:
        """
        Called when we receive a message for our receiver.

        :param event: The event which occurs when a message is received.
        """
        self.response = event.message
        self.connection.container.yield_()  # Wake up the wait() loop to handle the message.
