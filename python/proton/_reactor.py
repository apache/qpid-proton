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

import heapq
import json
import logging
import re
import os
import queue
from typing import Any, Dict, Iterator, Optional, List, Union, Callable, TYPE_CHECKING, Tuple, Type

try:
    from typing import Literal
except ImportError:
    # https://www.python.org/dev/peps/pep-0560/#class-getitem
    class GenericMeta(type):
        def __getitem__(self, item):
            pass

    class Literal(metaclass=GenericMeta):
        pass

import time
import traceback
import uuid
from functools import total_ordering

from cproton import PN_ACCEPTED, PN_EVENT_NONE

from ._data import Described, symbol, ulong
from ._delivery import Delivery
from ._endpoints import Connection, Endpoint, Link, Session, Terminus
from ._events import Collector, EventType, EventBase, Event
from ._exceptions import SSLUnavailable
from ._handler import Handler
from ._handlers import OutgoingMessageHandler, IOHandler
from ._io import IO
from ._message import Message
from ._transport import Transport, SSL, SSLDomain
from ._url import Url
from ._selectable import Selectable

if TYPE_CHECKING:
    from ._endpoints import Receiver, Sender
    from ._data import PythonAMQPData
    from ._handlers import TransactionHandler
    from socket import socket
    from types import TracebackType
    from uuid import UUID


_logger = logging.getLogger("proton")


def _generate_uuid() -> 'UUID':
    return uuid.uuid4()


def _now() -> float:
    return time.time()


@total_ordering
class Task(object):

    def __init__(self, reactor: 'Container', deadline: float, handler: Handler) -> None:
        self._deadline = deadline
        self._handler = handler
        self._reactor = reactor
        self._cancelled = False

    def __lt__(self, rhs: 'Task') -> bool:
        return self._deadline < rhs._deadline

    def cancel(self) -> None:
        self._cancelled = True

    @property
    def handler(self) -> Handler:
        return self._handler

    @property
    def container(self) -> 'Container':
        return self._reactor


class TimerSelectable(Selectable):

    def __init__(self, reactor: 'Container') -> None:
        super(TimerSelectable, self).__init__(None, reactor)

    def readable(self) -> None:
        pass

    def writable(self) -> None:
        pass

    def expired(self) -> None:
        self._reactor.timer_tick()
        self.deadline = self._reactor.timer_deadline
        self.update()


class Reactor(object):

    def __init__(self, *handlers, **kwargs) -> None:
        self._previous = PN_EVENT_NONE
        self._timeout = 0
        self.mark()
        self._yield = False
        self._stop = False
        self._collector = Collector()
        self._selectable = None
        self._selectables = 0
        self._global_handler = IOHandler()
        self._handler = Handler()
        self._timerheap = []
        self._timers = 0
        self.errors: List[Tuple[Type[BaseException], BaseException, 'TracebackType']] = []
        for h in handlers:
            self.handler.add(h, on_error=self.on_error)

    def on_error(self, info: Tuple[Type[BaseException], BaseException, 'TracebackType']) -> None:
        self.errors.append(info)
        self.yield_()

    # TODO: need to make this actually return a proxy which catches exceptions and calls
    # on error.
    # [Or arrange another way to deal with exceptions thrown by handlers]
    def _make_handler(self, handler: Handler) -> Handler:
        """
        Return a proxy handler that dispatches to the provided handler.

        If handler throws an exception then on_error is called with info
        """
        return handler

    @property
    def global_handler(self) -> Handler:
        return self._global_handler

    @global_handler.setter
    def global_handler(self, handler: Handler) -> None:
        self._global_handler = self._make_handler(handler)

    @property
    def timeout(self) -> float:
        return self._timeout

    @timeout.setter
    def timeout(self, secs: float) -> None:
        self._timeout = secs

    def yield_(self) -> None:
        self._yield = True

    def mark(self) -> float:
        """ This sets the reactor now instant to the current time """
        self._now = _now()
        return self._now

    @property
    def now(self) -> float:
        return self._now

    @property
    def handler(self) -> Handler:
        return self._handler

    @handler.setter
    def handler(self, handler: Handler) -> None:
        self._handler = self._make_handler(handler)

    def run(self) -> None:
        """
        Start the processing of events and messages for this container.
        """
        # TODO: Why do we timeout like this?
        self.timeout = 3.14159265359
        self.start()
        while self.process():
            pass
        self.stop()
        self.process()
        # TODO: This isn't correct if we ever run again
        self._global_handler = None
        self._handler = None

    # Cross thread reactor wakeup
    def wakeup(self) -> None:
        # TODO: Do this with pipe and write?
        #  os.write(self._wakeup[1], "x", 1);
        pass

    def start(self) -> None:
        self.push_event(self, Event.REACTOR_INIT)
        self._selectable = TimerSelectable(self)
        self._selectable.deadline = self.timer_deadline
        # TODO set up fd to read for wakeups - but problematic on windows
        #  self._selectable.fileno(self._wakeup[0])
        #  self._selectable.reading = True
        self.update(self._selectable)

    @property
    def quiesced(self) -> bool:
        event = self._collector.peek()
        if not event:
            return True
        if self._collector.more():
            return False
        return event.type is Event.REACTOR_QUIESCED

    def _check_errors(self) -> None:
        """ This """
        if self.errors:
            for exc, value, tb in self.errors[:-1]:
                traceback.print_exception(exc, value, tb)
            exc, value, tb = self.errors[-1]
            if value is None:
                value = exc()
            if tb is None:
                raise value
            else:
                raise value.with_traceback(tb)

    def process(self) -> bool:
        # result = pn_reactor_process(self._impl)
        # self._check_errors()
        # return result
        self.mark()
        previous = PN_EVENT_NONE
        while True:
            if self._yield:
                self._yield = False
                _logger.debug('%s Yielding', self)
                return True
            event = self._collector.peek()
            if event:
                _logger.debug('%s recvd Event: %r', self, event)
                type = event.type

                # regular handler
                handler = event.handler or self._handler
                event.dispatch(handler)

                event.dispatch(self._global_handler)

                previous = type
                self._previous = type
                self._collector.pop()
            elif not self._stop and (self._timers > 0 or self._selectables > 1):
                if previous is not Event.REACTOR_QUIESCED and self._previous is not Event.REACTOR_FINAL:
                    self.push_event(self, Event.REACTOR_QUIESCED)
                self.yield_()
            else:
                if self._selectable:
                    self._selectable.terminate()
                    self._selectable.update()
                    self._selectable = None
                else:
                    if self._previous is not Event.REACTOR_FINAL:
                        self.push_event(self, Event.REACTOR_FINAL)
                    _logger.debug('%s Stopping', self)
                    return False

    def stop(self) -> None:
        self._stop = True
        self._check_errors()

    def stop_events(self) -> None:
        self._collector.release()

    def schedule(self, delay: Union[float, int], handler: Handler) -> Task:
        """
        Schedule a task to run on this container after a given delay,
        and using the supplied handler.

        :param delay:
        :param handler:
        """
        himpl = self._make_handler(handler)
        task = Task(self, self._now + delay, himpl)
        heapq.heappush(self._timerheap, task)
        self._timers += 1
        deadline = self._timerheap[0]._deadline
        if self._selectable:
            self._selectable.deadline = deadline
            self.update(self._selectable)
        return task

    def timer_tick(self) -> None:
        while self._timers > 0:
            t = self._timerheap[0]
            if t._cancelled:
                heapq.heappop(self._timerheap)
                self._timers -= 1
            elif t._deadline > self._now:
                return
            else:
                heapq.heappop(self._timerheap)
                self._timers -= 1
                self.push_event(t, Event.TIMER_TASK)

    @property
    def timer_deadline(self) -> Optional[float]:
        while self._timers > 0:
            t = self._timerheap[0]
            if t._cancelled:
                heapq.heappop(self._timerheap)
                self._timers -= 1
            else:
                return t._deadline
        return None

    def acceptor(
            self,
            host: str,
            port: Union[str, Url.Port],
            handler: Optional[Handler] = None,
    ) -> 'Acceptor':
        impl = self._make_handler(handler)
        a = Acceptor(self, host, int(port), impl)
        if a:
            return a
        else:
            raise IOError("%s (%s:%s)" % (str(self.errors), host, port))

    def connection(self, handler: Optional[Handler] = None) -> Connection:
        """Deprecated: use connection_to_host() instead
        """
        impl = self._make_handler(handler)
        result = Connection()
        if impl:
            result.handler = impl
        result._reactor = self
        result.collect(self._collector)
        return result

    def connection_to_host(self, host, port, handler: Optional[Handler] = None) -> Connection:
        """Create an outgoing Connection that will be managed by the reactor.
        The reactor's pn_iohandler will create a socket connection to the host
        once the connection is opened.
        """
        conn = self.connection(handler)
        self.set_connection_host(conn, host, port)
        return conn

    def set_connection_host(self, connection: Connection, host, port) -> None:
        """Change the address used by the connection.  The address is
        used by the reactor's iohandler to create an outgoing socket
        connection.  This must be set prior to opening the connection.
        """
        connection.set_address(host, port)

    def get_connection_address(self, connection: Connection) -> str:
        """*Deprecated* in favor of the property proton.Connection.connected_address.
        This may be used to retrieve the remote peer address.
        :return: string containing the address in URL format or None if no
        address is available.  Use the proton.Url class to create a Url object
        from the returned value.
        """
        return connection.connected_address

    def selectable(
            self,
            handler: Optional[Union['Acceptor', 'EventInjector']] = None,
            delegate: Optional['socket'] = None
    ) -> Selectable:
        """
        NO IDEA!

        :param handler: no idea
        :type handler: ?
        :param delegate: no idea
        :type delegate: ?
        """
        if delegate is None:
            delegate = handler
        result = Selectable(delegate, self)
        result.handler = handler
        return result

    def update(self, selectable: Selectable) -> None:
        selectable.update()

    def push_event(
            self,
            obj: Union['Reactor', Task, 'Container', Selectable],
            etype: EventType
    ) -> None:
        self._collector.put(obj, etype)


class EventInjector(object):
    """
    Can be added to a :class:`Container` to allow events to be triggered by an
    external thread but handled on the event thread associated with
    the container. An instance of this class can be passed to the
    :meth:`Container.selectable` method in order to activate
    it. :meth:`close` should be called when it is no longer
    needed, to allow the event loop to end if needed.
    """

    def __init__(self) -> None:
        self.queue = queue.Queue()
        self.pipe = os.pipe()
        self._transport = None
        self._closed = False

    def trigger(self, event: 'ApplicationEvent') -> None:
        """
        Request that the given event be dispatched on the event thread
        of the container to which this EventInjector was added.

        :param event: Event to be injected
        :type event: :class:`proton.Event`, :class:`ApplicationEvent`
        """
        self.queue.put(event)
        os.write(self.pipe[1], b"!")

    def close(self) -> None:
        """
        Request that this EventInjector be closed. Existing events
        will be dispatched on the container's event dispatch thread,
        then this will be removed from the set of interest.
        """
        self._closed = True
        os.write(self.pipe[1], b"!")

    def fileno(self) -> int:
        return self.pipe[0]

    def on_selectable_init(self, event: Event) -> None:
        sel = event.context
        # sel.fileno(self.fileno())
        sel.reading = True
        sel.update()

    def on_selectable_readable(self, event: Event) -> None:
        s = event.context
        os.read(self.pipe[0], 512)
        while not self.queue.empty():
            requested = self.queue.get()
            s.push_event(requested.context, requested.type)
        if self._closed:
            s.terminate()
            s.update()


class ApplicationEvent(EventBase):
    """
    Application defined event, which can optionally be associated with
    an engine object and or an arbitrary subject. This produces
    extended event types - see :class:`proton.EventType` for details.

    :param typename: Event type name
    :param connection: Associates this event with a connection.
    :param session: Associates this event with a session.
    :param link: Associate this event with a link.
    :param delivery: Associate this event with a delivery.
    :param subject: Associate this event with an arbitrary object
    """

    TYPES = {}

    def __init__(
            self,
            typename: str,
            connection: Optional[Connection] = None,
            session: Optional[Session] = None,
            link: Optional[Link] = None,
            delivery: Optional[Delivery] = None,
            subject: Any = None
    ) -> None:
        if isinstance(typename, EventType):
            eventtype = typename
        else:
            try:
                eventtype = self.TYPES[typename]
            except KeyError:
                eventtype = EventType(typename)
                self.TYPES[typename] = eventtype
        super(ApplicationEvent, self).__init__(eventtype)
        self.connection = connection
        self.session = session
        self.link = link
        self.delivery = delivery
        if self.delivery:
            self.link = self.delivery.link
        if self.link:
            self.session = self.link.session
        if self.session:
            self.connection = self.session.connection
        self.subject = subject

    @property
    def context(self) -> 'ApplicationEvent':
        """
        A reference to this event.
        """
        return self

    def __repr__(self) -> str:
        objects = [self.connection, self.session, self.link, self.delivery, self.subject]
        return "%s(%s)" % (self.type, ", ".join([str(o) for o in objects if o is not None]))


class Transaction(object):
    """
    Tracks the state of an AMQP 1.0 local transaction. In typical usage, this
    object is not created directly, but is obtained through the event returned
    by :meth:`proton.handlers.TransactionHandler.on_transaction_declared` after
    a call to :meth:`proton.reactor.Container.declare_transaction`.

    To send messages under this transaction, use :meth:`send`.

    To receive messages under this transaction, call :meth:`accept` once the
    message is received (typically from the
    :meth:`proton.handlers.MessagingHandler.on_message` callback).

    To discharge the transaction, call either :meth:`commit`
    (for a successful transaction), or :meth:`abort` (for a failed transaction).
    """

    def __init__(
            self,
            txn_ctrl: 'Sender',
            handler: 'TransactionHandler',
            settle_before_discharge: bool = False,
    ) -> None:
        self.txn_ctrl = txn_ctrl
        self.handler = handler
        self.id = None
        self._declare = None
        self._discharge = None
        self.failed = False
        self._pending = []
        self.settle_before_discharge = settle_before_discharge
        self.declare()

    def commit(self) -> None:
        """
        Commit this transaction. Closes the transaction as a success.
        """
        self.discharge(False)

    def abort(self) -> None:
        """
        Abort or roll back this transaction. Closes the transaction as a failure,
        and reverses, or rolls back all actions (sent and received messages)
        performed under this transaction.
        """
        self.discharge(True)

    def declare(self) -> None:
        self._declare = self._send_ctrl(symbol(u'amqp:declare:list'), [None])

    def discharge(self, failed: bool) -> None:
        self.failed = failed
        self._discharge = self._send_ctrl(symbol(u'amqp:discharge:list'), [self.id, failed])

    def _send_ctrl(self, descriptor: 'PythonAMQPData', value: 'PythonAMQPData') -> Delivery:
        delivery = self.txn_ctrl.send(Message(body=Described(descriptor, value)))
        delivery.transaction = self
        return delivery

    def send(
            self,
            sender: 'Sender',
            msg: Message,
            tag: Optional[str] = None,
    ) -> Delivery:
        """
        Send a message under this transaction.

        :param sender: Link over which to send the message.
        :param msg: Message to be sent under this transaction.
        :param tag: The delivery tag
        :return: Delivery object for this message.
        """
        dlv = sender.send(msg, tag=tag)
        dlv.local.data = [self.id]
        dlv.update(0x34)
        return dlv

    def accept(self, delivery: Delivery) -> None:
        """
        Accept a received message under this transaction.

        :param delivery: Delivery object for the received message.
        """
        self.update(delivery, PN_ACCEPTED)
        if self.settle_before_discharge:
            delivery.settle()
        else:
            self._pending.append(delivery)

    def update(self, delivery: Delivery, state: Optional[ulong] = None) -> None:
        if state:
            delivery.local.data = [self.id, Described(ulong(state), [])]
            delivery.update(0x34)

    def _release_pending(self):
        for d in self._pending:
            d.update(Delivery.RELEASED)
            d.settle()
        self._clear_pending()

    def _clear_pending(self):
        self._pending = []

    def handle_outcome(self, event):
        if event.delivery == self._declare:
            if event.delivery.remote.data:
                self.id = event.delivery.remote.data[0]
                self.handler.on_transaction_declared(event)
            elif event.delivery.remote_state == Delivery.REJECTED:
                self.handler.on_transaction_declare_failed(event)
            else:
                _logger.warning("Unexpected outcome for declare: %s" % event.delivery.remote_state)
                self.handler.on_transaction_declare_failed(event)
        elif event.delivery == self._discharge:
            if event.delivery.remote_state == Delivery.REJECTED:
                if not self.failed:
                    self.handler.on_transaction_commit_failed(event)
                    self._release_pending()  # make this optional?
            else:
                if self.failed:
                    self.handler.on_transaction_aborted(event)
                    self._release_pending()
                else:
                    self.handler.on_transaction_committed(event)
            self._clear_pending()


class LinkOption(object):
    """
    Abstract interface for link configuration options
    """

    def apply(self, link: Link) -> None:
        """
        Subclasses will implement any configuration logic in this
        method
        """
        pass

    def test(self, link: Link) -> bool:
        """
        Subclasses can override this to selectively apply an option
        e.g. based on some link criteria
        """
        return True


class AtMostOnce(LinkOption):
    """
    Set at-most-once delivery semantics for message delivery. This is achieved by
    setting the sender link settle mode to :const:`proton.Link.SND_SETTLED`
    (ie pre-settled).
    """

    def apply(self, link: Link) -> None:
        """
        Set the at-most-once delivery semantics on the link.

        :param link: The link on which this option is to be applied.
        :type link: :class:`proton.Link`
        """
        link.snd_settle_mode = Link.SND_SETTLED


class AtLeastOnce(LinkOption):
    """
    Set at-least-once delivery semantics for message delivery. This is achieved
    by setting the sender link settle mode to :const:`proton.Link.SND_UNSETTLED`
    and the receiver link settle mode to :const:`proton.Link.RCV_FIRST`. This
    forces the receiver to settle all messages once they are successfully received.
    """

    def apply(self, link: Link) -> None:
        """
        Set the at-least-once delivery semantics on the link.

        :param link: The link on which this option is to be applied.
        :type link: :class:`proton.Link`
        """
        link.snd_settle_mode = Link.SND_UNSETTLED
        link.rcv_settle_mode = Link.RCV_FIRST


class SenderOption(LinkOption):
    """
    Abstract class for sender options.
    """

    def apply(self, sender: 'Sender') -> None:
        """
        Set the option on the sender.

        :param sender: The sender on which this option is to be applied.
        """
        pass

    def test(self, link: Link) -> bool:
        return link.is_sender


class ReceiverOption(LinkOption):
    """
    Abstract class for receiver options
    """

    def apply(self, receiver: 'Receiver') -> None:
        """
        Set the option on the receiver.

        :param receiver: The receiver on which this option is to be applied.
        """
        pass

    def test(self, link: Link) -> bool:
        return link.is_receiver


class DynamicNodeProperties(LinkOption):
    """
    Allows a map of link properties to be set on a link. The
    keys may be :class:`proton.symbol` or strings (in which case
    they will be converted to symbols before being applied).

    :param props: A map of link options to be applied to a link.
    """

    def __init__(self, props: dict = {}) -> None:
        self.properties = {}
        for k in props:
            if isinstance(k, symbol):
                self.properties[k] = props[k]
            else:
                self.properties[symbol(k)] = props[k]

    def apply(self, link: Link) -> None:
        """
        Set the map of properties on the specified link.

        :param link: The link on which this property map is to be set.
        """
        if link.is_receiver:
            link.source.properties.put_dict(self.properties)
        else:
            link.target.properties.put_dict(self.properties)


class Filter(ReceiverOption):
    """
    Receiver option which allows incoming messages to be filtered.

    :param filter_set: A map of filters with :class:`proton.symbol` keys
        containing the filter name, and the value a filter string.
    """

    def __init__(self, filter_set: Dict[symbol, Described] = {}) -> None:
        self.filter_set = filter_set

    def apply(self, receiver: 'Receiver') -> None:
        """
        Set the filter on the specified receiver.

        :param receiver: The receiver on which this filter is to be applied.
        """
        receiver.source.filter.put_dict(self.filter_set)


class Selector(Filter):
    """
    Configures a receiver with a message selector filter

    :param value: Selector filter string
    :param name: Name of the selector, defaults to ``"selector"``.
    """

    def __init__(self, value: str, name: str = 'selector') -> None:
        super(Selector, self).__init__({symbol(name): Described(
            symbol('apache.org:selector-filter:string'), value)})


class DurableSubscription(ReceiverOption):
    """
    Receiver option which sets both the configuration and delivery state
    to durable. This is achieved by setting the receiver's source durability
    to :const:`proton.Terminus.DELIVERIES` and the source expiry policy to
    :const:`proton.Terminus.EXPIRE_NEVER`.
    """

    def apply(self, receiver: 'Receiver'):
        """
        Set durability on the specified receiver.

        :param receiver: The receiver on which durability is to be set.
        """
        receiver.source.durability = Terminus.DELIVERIES
        receiver.source.expiry_policy = Terminus.EXPIRE_NEVER


class Move(ReceiverOption):
    """
    Receiver option which moves messages to the receiver (rather than copying).
    This has the effect of distributing the incoming messages between the
    receivers. This is achieved by setting the receiver source distribution
    mode to :const:`proton.Terminus.DIST_MODE_MOVE`.
    """

    def apply(self, receiver: 'Receiver'):
        """
        Set message move semantics on the specified receiver.

        :param receiver: The receiver on which message move semantics is to be set.
        """
        receiver.source.distribution_mode = Terminus.DIST_MODE_MOVE


class Copy(ReceiverOption):
    """
    Receiver option which copies messages to the receiver. This ensures that all
    receivers receive all incoming messages, no matter how many receivers there
    are. This is achieved by setting the receiver source distribution mode to
    :const:`proton.Terminus.DIST_MODE_COPY`.
    """

    def apply(self, receiver: 'Receiver'):
        """
        Set message copy semantics on the specified receiver.

        :param receiver: The receiver on which message copy semantics is to be set.
        """
        receiver.source.distribution_mode = Terminus.DIST_MODE_COPY


def _apply_link_options(
        options: Optional[Union[LinkOption, List[LinkOption]]],
        link: Union['Sender', 'Receiver']
) -> None:
    if options:
        if isinstance(options, list):
            for o in options:
                if o.test(link):
                    o.apply(link)
        else:
            if options.test(link):
                options.apply(link)


def _create_session(connection: Connection, handler: Optional[Handler] = None) -> Session:
    session = connection.session()
    session.open()
    return session


def _get_attr(target: Any, name: str) -> Optional[Any]:
    if hasattr(target, name):
        return getattr(target, name)
    else:
        return None


class SessionPerConnection(object):
    def __init__(self) -> None:
        self._default_session = None

    def session(self, connection: Connection) -> Session:
        if not self._default_session:
            self._default_session = _create_session(connection)
        return self._default_session


class GlobalOverrides(Handler):
    """
    Internal handler that triggers the necessary socket connect for an
    opened connection.
    """

    def __init__(self, base: IOHandler) -> None:
        self.base = base

    def on_unhandled(self, name: str, event: Event) -> None:
        if not self._override(event):
            event.dispatch(self.base)

    def _override(self, event: Event) -> Optional[bool]:
        conn = event.connection
        return conn and hasattr(conn, '_overrides') and event.dispatch(conn._overrides)


class Acceptor(Handler):

    def __init__(self, reactor: 'Container', host: str, port: int, handler: Optional[Handler] = None) -> None:
        self._ssl_domain = None
        self._reactor = reactor
        self._handler = handler
        sock = IO.listen(host, port)
        s = reactor.selectable(handler=self, delegate=sock)
        s.reading = True
        s._transport = None
        self._selectable = s
        reactor.update(s)

    def set_ssl_domain(self, ssl_domain: SSLDomain) -> None:
        self._ssl_domain = ssl_domain

    def close(self) -> None:
        if not self._selectable.is_terminal:
            self._selectable.terminate()
            self._selectable.update()

    def on_selectable_readable(self, event: Event) -> None:
        s = event.selectable

        sock, name = IO.accept(self._selectable)
        _logger.info("Accepted connection from %s", name)

        r = self._reactor
        handler = self._handler or r.handler
        c = r.connection(handler)
        c._acceptor = self
        c.url = Url(host=name[0], port=name[1])
        t = Transport(Transport.SERVER)
        if self._ssl_domain:
            t.ssl(self._ssl_domain)
        t.bind(c)

        s = r.selectable(delegate=sock)
        s._transport = t
        t._selectable = s
        IOHandler.update(t, s, r.now)


def delay_iter(
        initial: float = 0.1,
        factor: float = 2.0,
        max_delay: float = 10.0,
        max_tries: Optional[int] = None
) -> Iterator[float]:
    """
    iterator yielding the next delay in the sequence of delays. The first
    delay is 0 seconds, the second 0.1 seconds, and each subsequent
    call to :meth:`next` doubles the next delay period until a
    maximum value of 10 seconds is reached.
    """
    yield 0.0
    tries = 1
    delay = initial
    while max_tries is None or tries < max_tries:
        yield delay
        tries += 1
        delay = min(max_delay, factor * delay)


class Backoff(object):
    """
    A reconnect strategy involving an increasing delay between
    retries, up to a maximum or 10 seconds. Repeated calls
    to :meth:`next` returns a value for the next delay, starting
    with an initial value of 0 seconds.
    """

    def __init__(self, **kwargs) -> None:
        self.kwargs = kwargs
        self.iter = delay_iter(**self.kwargs)

    def __iter__(self) -> Iterator[float]:
        return self.iter


def make_backoff_wrapper(
        backoff: Optional[Union[List[Union[float, int]], bool, Backoff]]
) -> Optional[Union[List[Union[float, int]], bool, Backoff]]:
    """
    Make a wrapper for a backoff object:
    If the object conforms to the old protocol (has reset and next methods) then
    wrap it in an iterable that returns an iterator suitable for the new backoff approach
    otherwise assume it is fine as it is!
    """
    class WrappedBackoff(object):
        def __init__(self, backoff):
            self.backoff = backoff

        def __iter__(self):
            self.backoff.reset()
            return self

        def __next__(self):
            return self.backoff.next()
    if hasattr(backoff, 'reset') and hasattr(backoff, 'next'):
        return WrappedBackoff(backoff)
    else:
        return backoff


class Urls(object):
    def __init__(self, values: List[Union[Url, str]]) -> None:
        self.values = [Url(v) for v in values]

    def __iter__(self) -> Iterator[Url]:
        return iter(self.values)


class _Connector(Handler):
    """
    Internal handler that triggers the necessary socket connect for an
    opened connection.
    """

    def __init__(self, connection: Connection) -> None:
        self.connection = connection
        self.address = None
        self.heartbeat = None
        self.reconnect = None
        self.ssl_domain = None
        self.allow_insecure_mechs = True
        self.allowed_mechs = None
        self.sasl_enabled = True
        self.user = None
        self.password = None
        self.virtual_host = None
        self.ssl_sni = None
        self.max_frame_size = None
        self._connect_sequence = None
        self._next_url = None

    def _connect(self, connection: Connection, url: Url) -> None:
        connection.url = url
        # if virtual-host not set, use host from address as default
        if self.virtual_host is None:
            connection.hostname = url.host
        _logger.info("Connecting to %r..." % url)

        transport = Transport()
        if self.sasl_enabled:
            sasl = transport.sasl()
            sasl.allow_insecure_mechs = self.allow_insecure_mechs
            if url.username:
                connection.user = url.username
            elif self.user:
                connection.user = self.user
            if url.password:
                connection.password = url.password
            elif self.password:
                connection.password = self.password
            if self.allowed_mechs:
                sasl.allowed_mechs(self.allowed_mechs)
        transport.bind(connection)
        if self.heartbeat:
            transport.idle_timeout = self.heartbeat
        if url.scheme == 'amqps':
            if not self.ssl_domain:
                raise SSLUnavailable("amqps: SSL libraries not found")
            self.ssl = SSL(transport, self.ssl_domain)
            self.ssl.peer_hostname = self.ssl_sni or self.virtual_host or url.host
        if self.max_frame_size:
            transport.max_frame_size = self.max_frame_size

    def on_connection_local_open(self, event: Event) -> None:
        if self.reconnect is None:
            self._connect_sequence = ((delay, url) for delay in delay_iter() for url in self.address)
        elif self.reconnect is False:
            self._connect_sequence = ((delay, url) for delay in delay_iter(max_tries=1) for url in self.address)
        else:
            self._connect_sequence = ((delay, url) for delay in self.reconnect for url in self.address)
        _, url = next(self._connect_sequence)  # Ignore delay as we assume first delay must be 0
        self._connect(event.connection, url)

    def on_connection_remote_open(self, event: Event) -> None:
        _logger.info("Connected to %s" % event.connection.hostname)
        if self.reconnect is None:
            self._connect_sequence = ((delay, url) for delay in delay_iter() for url in self.address)
        elif self.reconnect:
            self._connect_sequence = ((delay, url) for delay in self.reconnect for url in self.address)
        else:
            self._connect_sequence = None  # Help take out the garbage

    def on_transport_closed(self, event: Event) -> None:
        if self.connection is None:

            return

        if not self.connection.state & Endpoint.LOCAL_ACTIVE:
            _logger.info("Disconnected, already closed")
        elif self.reconnect is False:
            _logger.info("Disconnected, reconnect disabled")
        else:
            try:
                event.transport.unbind()
                delay, url = next(self._connect_sequence)
                if delay == 0:
                    _logger.info("Disconnected, reconnecting immediately...")
                    self._connect(self.connection, url)
                    return
                else:
                    _logger.info("Disconnected will try to reconnect after %s seconds" % delay)
                    self._next_url = url
                    event.reactor.schedule(delay, self)
                    return
            except StopIteration:
                _logger.info("Disconnected, giving up retrying")

        # See connector.cpp: conn.free()/pn_connection_release() here?
        self.connection = None

    def on_timer_task(self, event: Event) -> None:
        if self._next_url:
            self._connect(self.connection, self._next_url)
            self._next_url = None


class SSLConfig(object):
    def __init__(self) -> None:
        self.client = SSLDomain(SSLDomain.MODE_CLIENT)
        self.server = SSLDomain(SSLDomain.MODE_SERVER)

    def set_credentials(self, cert_file, key_file, password):
        self.client.set_credentials(cert_file, key_file, password)
        self.server.set_credentials(cert_file, key_file, password)

    def set_trusted_ca_db(self, certificate_db):
        self.client.set_trusted_ca_db(certificate_db)
        self.server.set_trusted_ca_db(certificate_db)


def _find_config_file() -> Optional[str]:
    confname = 'connect.json'
    confpath = ['.', os.path.expanduser('~/.config/messaging'), '/etc/messaging']
    for d in confpath:
        f = os.path.join(d, confname)
        if os.path.isfile(f):
            return f
    return None


def _get_default_config() -> Dict[str, Any]:
    conf = os.environ.get('MESSAGING_CONNECT_FILE') or _find_config_file()
    if conf and os.path.isfile(conf):
        with open(conf, 'r') as f:
            json_text = f.read()
            json_text = _strip_json_comments(json_text)
            return json.loads(json_text)
    else:
        return {}


def _strip_json_comments(json_text: str) -> str:
    """This strips c-style comments from text, taking into account '/*comments*/' and '//comments'
    nested inside a string etc."""
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " "  # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"', re.DOTALL | re.MULTILINE)
    return re.sub(pattern, replacer, json_text)


def _get_default_port_for_scheme(scheme: str) -> int:
    if scheme == 'amqps':
        return 5671
    else:
        return 5672


class Container(Reactor):
    """
    A representation of the AMQP concept of a 'container', which
    loosely speaking is something that establishes links to or from
    another container, over which messages are transfered. This is
    an extension to the Reactor class that adds convenience methods
    for creating connections and sender- or receiver- links.
    """

    def __init__(self, *handlers, **kwargs) -> None:
        super(Container, self).__init__(*handlers, **kwargs)
        if "impl" not in kwargs:
            try:
                self.ssl = SSLConfig()
            except SSLUnavailable:
                self.ssl = None
            self.global_handler = GlobalOverrides(kwargs.get('global_handler', self.global_handler))
            self.trigger = None
            self.container_id = kwargs.get('container_id', str(_generate_uuid()))
            self.allow_insecure_mechs = True
            self.allowed_mechs = None
            self.sasl_enabled = True
            self.user = None
            self.password = None

    def connect(
            self,
            url: Optional[Union[str, Url]] = None,
            urls: Optional[List[str]] = None,
            address: Optional[str] = None,
            handler: Optional[Handler] = None,
            reconnect: Union[None, Literal[False], Backoff] = None,
            heartbeat: Optional[float] = None,
            ssl_domain: Optional[SSLDomain] = None,
            **kwargs
    ) -> Connection:
        """
        Initiates the establishment of an AMQP connection.

        An optional JSON configuration file may be used to specify some connection
        parameters. If present, these will override some of those given in this call
        (see note below). Some connection parameters (for SSL/TLS) can only be
        provided through this file. The configuration file is located by searching
        for it as follows:

            1.  The location set in the environment variable ``MESSAGING_CONNECT_FILE``
            2.  ``./connect.json``
            3.  ``~/.config/messaging/connect.json``
            4.  ``/etc/messaging/connect.json``

        To use SSL/TLS for encryption (when an ``amqps`` URL scheme is used), the above
        configuration file must contain a ``tls`` submap containing the following
        configuration entries (See :class:`proton.SSLDomain` for details):

        *   ``ca``: Path to a database of trusted CAs that the server will advertise.
        *   ``cert``: Path to a file/database containing the identifying certificate.
        *   ``key``: An optional key to access the identifying certificate.
        *   ``verify``: If ``False``, do not verify the peer name
            (:const:`proton.SSLDomain.ANONYMOUS_PEER`) or certificate.  By default
            (or if ``True``) verify the peer name and certificate using the
            ``ca`` above (:const:`proton.SSLDomain.VERIFY_PEER_NAME`).

        :param url: URL string of process to connect to
        :param urls: list of URL strings of process to try to connect to

        :param reconnect: Reconnect is enabled by default.  You can
            pass in an instance of :class:`Backoff` to control reconnect behavior.
            A value of ``False`` will prevent the library from automatically
            trying to reconnect if the underlying socket is disconnected
            before the connection has been closed.

        :param heartbeat: A value in seconds indicating the
            desired frequency of heartbeats used to test the underlying
            socket is alive.

        :param ssl_domain: SSL configuration.

        :param handler: a connection scoped handler that will be
            called to process any events in the scope of this connection
            or its child links.

        :param kwargs:

            *   ``sasl_enabled`` (``bool``), which determines whether a sasl layer
                is used for the connection.
            *   ``allowed_mechs`` (``str``), an optional string specifying the
                SASL mechanisms allowed for this  connection; the value is a
                space-separated list of mechanism  names; the mechanisms allowed
                by default are determined by your SASL library and system
                configuration, with two exceptions: ``GSSAPI`` and ``GSS-SPNEGO``
                are disabled by default; to enable them, you must explicitly add
                them using this option; clients must set the allowed mechanisms
                before the outgoing connection is attempted; servers must set
                them before the listening connection is setup.
            *   ``allow_insecure_mechs`` (``bool``), a flag indicating whether insecure
                mechanisms, such as PLAIN over a non-encrypted socket, are
                allowed.
            *   ``password`` (``str``), the authentication secret. Ignored without ``user``
                kwarg also being present.
            *   ``user`` (``str``), the user to authenticate.
            *   ``virtual_host`` (``str``), the hostname to set in the Open performative
                used by peer to determine the correct back-end service for
                the client; if ``virtual_host`` is not supplied the host field
                from the URL is used instead.
            *   ``offered_capabilities``, a list of capabilities being offered to the
                peer. The list must contain symbols (or strings, which will be converted
                to symbols).
            *   ``desired_capabilities``, a list of capabilities desired from the peer.
                The list must contain symbols (or strings, which will be converted
                to symbols).
            *   ``properties``, a list of connection properties. This must be a map
                with symbol keys (or string keys, which will be converted to symbol keys).
            *   ``sni`` (``str``), a hostname to use with SSL/TLS Server Name Indication (SNI)
            *   ``max_frame_size`` (``int``), the maximum allowable TCP packet size between the
                peers.

        :return: A new connection object.

        .. note:: Only one of ``url`` or ``urls`` should be specified.

        .. note:: The following kwargs will be overridden by the values found
            in the JSON configuration file (if they exist there):

            * ``password``
            * ``user``

            and the following kwargs will be overridden by the values found in the ``sasl``
            sub-map of the above configuration file (if they exist there):

            * ``sasl_enabled``
            * ``allowed_mechs``
        """
        if not url and not urls and not address:
            config = _get_default_config()
            scheme = config.get('scheme', 'amqps')
            _url = "%s://%s:%s" % (scheme, config.get('host', 'localhost'),
                                   config.get('port', _get_default_port_for_scheme(scheme)))
            _ssl_domain = None
            _kwargs = kwargs
            if config.get('user'):
                _kwargs['user'] = config.get('user')
                if config.get('password'):
                    _kwargs['password'] = config.get('password')
            sasl_config = config.get('sasl', {})
            _kwargs['sasl_enabled'] = sasl_config.get('enabled', True)
            if sasl_config.get('mechanisms'):
                _kwargs['allowed_mechs'] = sasl_config.get('mechanisms')
            tls_config = config.get('tls', {})
            if scheme == 'amqps':
                _ssl_domain = SSLDomain(SSLDomain.MODE_CLIENT)
                ca = tls_config.get('ca')
                cert = tls_config.get('cert')
                key = tls_config.get('key')
                verify = tls_config.get('verify', True)
                if ca:
                    _ssl_domain.set_trusted_ca_db(str(ca))
                if not verify:
                    _ssl_domain.set_peer_authentication(SSLDomain.ANONYMOUS_PEER, None)
                if cert and key:
                    _ssl_domain.set_credentials(str(cert), str(key), None)

            return self._connect(_url, handler=handler, reconnect=reconnect,
                                 heartbeat=heartbeat, ssl_domain=_ssl_domain, **_kwargs)
        else:
            return self._connect(url=url, urls=urls, handler=handler, reconnect=reconnect,
                                 heartbeat=heartbeat, ssl_domain=ssl_domain, **kwargs)

    def _connect(
            self,
            url: Optional[Union[str, Url]] = None,
            urls: Optional[List[str]] = None,
            handler: Optional['Handler'] = None,
            reconnect: Optional[Union[List[Union[float, int]], bool, Backoff]] = None,
            heartbeat: None = None,
            ssl_domain: Optional[SSLDomain] = None,
            **kwargs
    ) -> Connection:
        conn = self.connection(handler)
        conn.container = kwargs.get('container_id', self.container_id) or str(_generate_uuid())
        conn.offered_capabilities = kwargs.get('offered_capabilities')
        conn.desired_capabilities = kwargs.get('desired_capabilities')
        conn.properties = kwargs.get('properties')

        connector = _Connector(conn)
        connector.allow_insecure_mechs = kwargs.get('allow_insecure_mechs', self.allow_insecure_mechs)
        connector.allowed_mechs = kwargs.get('allowed_mechs', self.allowed_mechs)
        connector.sasl_enabled = kwargs.get('sasl_enabled', self.sasl_enabled)
        connector.user = kwargs.get('user', self.user)
        connector.password = kwargs.get('password', self.password)
        connector.virtual_host = kwargs.get('virtual_host')
        if connector.virtual_host:
            # only set hostname if virtual-host is a non-empty string
            conn.hostname = connector.virtual_host
        connector.ssl_sni = kwargs.get('sni')
        connector.max_frame_size = kwargs.get('max_frame_size')

        conn._overrides = connector
        if url:
            connector.address = Urls([url])
        elif urls:
            connector.address = Urls(urls)
        else:
            raise ValueError("One of url or urls required")
        if heartbeat:
            connector.heartbeat = heartbeat

        connector.reconnect = make_backoff_wrapper(reconnect)

        # use container's default client domain if none specified.  This is
        # only necessary of the URL specifies the "amqps:" scheme
        connector.ssl_domain = ssl_domain or (self.ssl and self.ssl.client)
        conn._session_policy = SessionPerConnection()  # todo: make configurable
        conn.open()
        return conn

    def _get_id(self, container: str, remote: Optional[str], local: Optional[str]) -> str:
        if local and remote:
            return "%s-%s-%s" % (container, remote, local)
        elif local:
            return "%s-%s" % (container, local)
        elif remote:
            return "%s-%s" % (container, remote)
        else:
            return "%s-%s" % (container, str(_generate_uuid()))

    def _get_session(self, context: Connection) -> Session:
        if isinstance(context, Url):
            return self._get_session(self.connect(url=context))
        elif isinstance(context, Session):
            return context
        elif isinstance(context, Connection):
            if hasattr(context, '_session_policy'):
                return context._session_policy.session(context)
            else:
                return _create_session(context)
        else:
            return context.session()

    def create_sender(
            self,
            context: Union[str, Url, Connection],
            target: Optional[str] = None,
            source: Optional[str] = None,
            name: Optional[str] = None,
            handler: Optional[Handler] = None,
            tags: Optional[Callable[[], bytes]] = None,
            options: Optional[Union['SenderOption', List['SenderOption'], 'LinkOption', List['LinkOption']]] = None
    ) -> 'Sender':
        """
        Initiates the establishment of a link over which messages can
        be sent.

        There are two patterns of use:

        1.  A connection can be passed as the first argument, in which
            case the link is established on that connection. In this case
            the target address can be specified as the second argument (or
            as a keyword argument). The source address can also be specified
            if desired.

        2.  Alternatively a URL can be passed as the first argument. In
            this case a new connection will be established on which the link
            will be attached. If a path is specified and the target is not,
            then the path of the URL is used as the target address.

        The name of the link may be specified if desired, otherwise a
        unique name will be generated.

        Various :class:`LinkOption` s can be specified to further control the
        attachment.

        :param context: A connection object or a URL.
        :param target: Address of target node.
        :param source: Address of source node.
        :param name: Sender name.
        :param handler: Event handler for this sender.
        :param tags: Function to generate tags for this sender of the form ``def simple_tags():`` and returns a ``bytes`` type
        :param options: A single option, or a list of sender options

        :return: New sender instance.
        """
        if isinstance(context, str):
            context = Url(context)
        if isinstance(context, Url) and not target:
            target = context.path
        session = self._get_session(context)
        snd = session.sender(name or self._get_id(session.connection.container, target, source))
        if source:
            snd.source.address = source
        if target:
            snd.target.address = target
        if handler is not None:
            snd.handler = handler
        if tags:
            snd.tag_generator = tags
        _apply_link_options(options, snd)
        snd.open()
        return snd

    def create_receiver(
            self,
            context: Union[Connection, Url, str],
            source: Optional[str] = None,
            target: Optional[str] = None,
            name: Optional[str] = None,
            dynamic: bool = False,
            handler: Optional[Handler] = None,
            options: Optional[Union[ReceiverOption, List[ReceiverOption], LinkOption, List[LinkOption]]] = None
    ) -> 'Receiver':
        """
        Initiates the establishment of a link over which messages can
        be received (aka a subscription).

        There are two patterns of use:

        (1) A connection can be passed as the first argument, in which
        case the link is established on that connection. In this case
        the source address can be specified as the second argument (or
        as a keyword argument). The target address can also be specified
        if desired.

        (2) Alternatively a URL can be passed as the first argument. In
        this case a new connection will be established on which the link
        will be attached. If a path is specified and the source is not,
        then the path of the URL is used as the target address.

        The name of the link may be specified if desired, otherwise a
        unique name will be generated.

        Various :class:`LinkOption` s can be specified to further control the
        attachment.

        :param context: A connection object or a URL.
        :param source: Address of source node.
        :param target: Address of target node.
        :param name: Receiver name.
        :param dynamic: If ``True``, indicates dynamic creation of the receiver.
        :param handler: Event handler for this receiver.
        :param options: A single option, or a list of receiver options

        :return: New receiver instance.
        """
        if isinstance(context, str):
            context = Url(context)
        if isinstance(context, Url) and not source:
            source = context.path
        session = self._get_session(context)
        rcv = session.receiver(name or self._get_id(session.connection.container, source, target))
        if source:
            rcv.source.address = source
        if dynamic:
            rcv.source.dynamic = True
        if target:
            rcv.target.address = target
        if handler is not None:
            rcv.handler = handler
        _apply_link_options(options, rcv)
        rcv.open()
        return rcv

    def declare_transaction(
            self,
            context: Connection,
            handler: Optional['TransactionHandler'] = None,
            settle_before_discharge: bool = False
    ) -> Transaction:
        """
        Declare a local transaction.

        :param context: Context for the transaction, usually the connection.
        :param handler: Handler for transactional events.
        :param settle_before_discharge: Settle all transaction control messages before
            the transaction is discharged.
        """
        if not _get_attr(context, '_txn_ctrl'):
            class InternalTransactionHandler(OutgoingMessageHandler):
                def __init__(self):
                    super(InternalTransactionHandler, self).__init__(auto_settle=True)

                def on_settled(self, event):
                    if hasattr(event.delivery, "transaction"):
                        event.transaction = event.delivery.transaction
                        event.delivery.transaction.handle_outcome(event)

                def on_unhandled(self, method, event):
                    if handler:
                        event.dispatch(handler)

            context._txn_ctrl = self.create_sender(context, None, name='txn-ctrl', handler=InternalTransactionHandler())
            context._txn_ctrl.target.type = Terminus.COORDINATOR
            context._txn_ctrl.target.capabilities.put_object(symbol(u'amqp:local-transactions'))
        return Transaction(context._txn_ctrl, handler, settle_before_discharge)

    def listen(self, url: Union[str, Url], ssl_domain: Optional[SSLDomain] = None) -> Acceptor:
        """
        Initiates a server socket, accepting incoming AMQP connections
        on the interface and port specified.

        :param url: URL on which to listen for incoming AMQP connections.
        :param ssl_domain: SSL configuration object if SSL is to be used, ``None`` otherwise.
        """
        url = Url(url)
        acceptor = self.acceptor(url.host, url.port)
        ssl_config = ssl_domain
        if not ssl_config and url.scheme == 'amqps':
            # use container's default server domain
            if self.ssl:
                ssl_config = self.ssl.server
            else:
                raise SSLUnavailable("amqps: SSL libraries not found")
        if ssl_config:
            acceptor.set_ssl_domain(ssl_config)
        return acceptor

    def do_work(self, timeout: Optional[float] = None) -> bool:
        if timeout:
            self.timeout = timeout
        return self.process()
