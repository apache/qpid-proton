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

#from functools import total_ordering
import heapq
import json
import logging
import re
import os
import time
import traceback
import uuid

from cproton import PN_PYREF, PN_ACCEPTED, PN_EVENT_NONE

from ._delivery import  Delivery
from ._endpoints import Connection, Endpoint, Link, Session, Terminus
from ._exceptions import SSLUnavailable
from ._data import Described, symbol, ulong
from ._message import  Message
from ._transport import Transport, SSL, SSLDomain
from ._url import Url
from ._common import isstring, unicode2utf8, utf82unicode
from ._events import Collector, EventType, EventBase, Handler, Event
from ._selectable import Selectable

from ._handlers import OutgoingMessageHandler, IOHandler

from ._io import IO, PN_INVALID_SOCKET

from . import _compat
from ._compat import queue


_logger = logging.getLogger("proton")


def _generate_uuid():
    return uuid.uuid4()


def _now():
    return time.time()

#@total_ordering
class Task(object):

    def __init__(self, reactor, deadline, handler):
        self._deadline = deadline
        self._handler = handler
        self._reactor = reactor
        self._cancelled = False

    def __lt__(self, rhs):
        return self._deadline < rhs._deadline

    def cancel(self):
        self._cancelled = True

    @property
    def handler(self):
        return self._handler

    @property
    def container(self):
        return self._reactor

class TimerSelectable(Selectable):

    def __init__(self, reactor, collector):
        super(TimerSelectable, self).__init__(None, reactor)
        self.collect(collector)
        collector.put(self, Event.SELECTABLE_INIT)

    def fileno(self):
        return PN_INVALID_SOCKET

    def readable(self):
        pass

    def writable(self):
        pass

    def expired(self):
        self._reactor.timer_tick()
        self.deadline = self._reactor.timer_deadline
        self.update()

class Reactor(object):

    def __init__(self, *handlers, **kwargs):
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
        self.errors = []
        for h in handlers:
            self.handler.add(h, on_error=self.on_error)

    def on_error(self, info):
        self.errors.append(info)
        self.yield_()

    # TODO: need to make this actually return a proxy which catches exceptions and calls
    # on error.
    # [Or arrange another way to deal with exceptions thrown by handlers]
    def _make_handler(self, handler):
        """
        Return a proxy handler that dispatches to the provided handler.

        If handler throws an exception then on_error is called with info
        """
        return handler

    def _get_global(self):
        return self._global_handler

    def _set_global(self, handler):
        self._global_handler = self._make_handler(handler)

    global_handler = property(_get_global, _set_global)

    def _get_timeout(self):
        return self._timeout

    def _set_timeout(self, secs):
        self._timeout = secs

    timeout = property(_get_timeout, _set_timeout)

    def yield_(self):
        self._yield = True

    def mark(self):
        """ This sets the reactor now instant to the current time """
        self._now = _now()
        return self._now

    @property
    def now(self):
        return self._now

    def _get_handler(self):
        return self._handler

    def _set_handler(self, handler):
        self._handler = self._make_handler(handler)

    handler = property(_get_handler, _set_handler)

    def run(self):
        # TODO: Why do we timeout like this?
        self.timeout = 3.14159265359
        self.start()
        while self.process(): pass
        self.stop()
        self.process()
        # TODO: This isn't correct if we ever run again
        self._global_handler = None
        self._handler = None

    # Cross thread reactor wakeup
    def wakeup(self):
        # TODO: Do this with pipe and write?
        #os.write(self._wakeup[1], "x", 1);
        pass

    def start(self):
        self.push_event(self, Event.REACTOR_INIT)
        self._selectable = TimerSelectable(self, self._collector)
        self._selectable.deadline = self.timer_deadline
        # TODO set up fd to read for wakeups - but problematic on windows
        #self._selectable.fileno(self._wakeup[0])
        #self._selectable.reading = True
        self.update(self._selectable)

    @property
    def quiesced(self):
        event = self._collector.peek()
        if not event:
            return True
        if self._collector.more():
            return False
        return event.type is Event.REACTOR_QUIESCED

    def _check_errors(self):
        """ This """
        if self.errors:
            for exc, value, tb in self.errors[:-1]:
                traceback.print_exception(exc, value, tb)
            exc, value, tb = self.errors[-1]
            _compat.raise_(exc, value, tb)

    def process(self):
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
                    self.update(self._selectable)
                    self._selectable = None
                else:
                    if self._previous is not Event.REACTOR_FINAL:
                        self.push_event(self, Event.REACTOR_FINAL)
                    _logger.debug('%s Stopping', self)
                    return False

    def stop(self):
        self._stop = True
        self._check_errors()

    def stop_events(self):
        self._collector.release()

    def schedule(self, delay, handler):
        himpl = self._make_handler(handler)
        task = Task(self, self._now+delay, himpl)
        heapq.heappush(self._timerheap, task)
        self._timers += 1
        deadline = self._timerheap[0]._deadline
        if self._selectable:
            self._selectable.deadline = deadline
            self.update(self._selectable)
        return task

    def timer_tick(self):
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
    def timer_deadline(self):
        while self._timers > 0:
            t = self._timerheap[0]
            if t._cancelled:
                heapq.heappop(self._timerheap)
                self._timers -= 1
            else:
                return t._deadline
        return None

    def acceptor(self, host, port, handler=None):
        impl = self._make_handler(handler)
        a = Acceptor(self, unicode2utf8(host), int(port), impl)
        if a:
            return a
        else:
            raise IOError("%s (%s:%s)" % (str(self.errors), host, port))

    def connection(self, handler=None):
        """Deprecated: use connection_to_host() instead
        """
        impl = self._make_handler(handler)
        result = Connection()
        if impl:
            result.handler = impl
        result._reactor = self
        result.collect(self._collector)
        return result

    def connection_to_host(self, host, port, handler=None):
        """Create an outgoing Connection that will be managed by the reactor.
        The reactor's pn_iohandler will create a socket connection to the host
        once the connection is opened.
        """
        conn = self.connection(handler)
        self.set_connection_host(conn, host, port)
        return conn

    def set_connection_host(self, connection, host, port):
        """Change the address used by the connection.  The address is
        used by the reactor's iohandler to create an outgoing socket
        connection.  This must be set prior to opening the connection.
        """
        connection.set_address(host, port)

    def get_connection_address(self, connection):
        """This may be used to retrieve the remote peer address.
        @return: string containing the address in URL format or None if no
        address is available.  Use the proton.Url class to create a Url object
        from the returned value.
        """
        _url = connection.get_address()
        return utf82unicode(_url)

    def selectable(self, handler=None, delegate=None):
        if delegate is None:
            delegate = handler
        result = Selectable(delegate, self)
        result.collect(self._collector)
        result.handler = handler
        result.push_event(result, Event.SELECTABLE_INIT)
        return result

    def update(self, selectable):
        selectable.update()

    def push_event(self, obj, etype):
        self._collector.put(obj, etype)


class EventInjector(object):
    """
    Can be added to a reactor to allow events to be triggered by an
    external thread but handled on the event thread associated with
    the reactor. An instance of this class can be passed to the
    Reactor.selectable() method of the reactor in order to activate
    it. The close() method should be called when it is no longer
    needed, to allow the event loop to end if needed.
    """

    def __init__(self):
        self.queue = queue.Queue()
        self.pipe = os.pipe()
        self._transport = None
        self._closed = False

    def trigger(self, event):
        """
        Request that the given event be dispatched on the event thread
        of the reactor to which this EventInjector was added.
        """
        self.queue.put(event)
        os.write(self.pipe[1], b"!")

    def close(self):
        """
        Request that this EventInjector be closed. Existing events
        will be dispatched on the reactors event dispatch thread,
        then this will be removed from the set of interest.
        """
        self._closed = True
        os.write(self.pipe[1], b"!")

    def fileno(self):
        return self.pipe[0]

    def on_selectable_init(self, event):
        sel = event.context
        #sel.fileno(self.fileno())
        sel.reading = True
        sel.update()

    def on_selectable_readable(self, event):
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
    an engine object and or an arbitrary subject
    """

    def __init__(self, typename, connection=None, session=None, link=None, delivery=None, subject=None):
        super(ApplicationEvent, self).__init__(EventType(typename))
        self.clazz = PN_PYREF
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
    def context(self):
        return self

    def __repr__(self):
        objects = [self.connection, self.session, self.link, self.delivery, self.subject]
        return "%s(%s)" % (self.type, ", ".join([str(o) for o in objects if o is not None]))


class Transaction(object):
    """
    Class to track state of an AMQP 1.0 transaction.
    """

    def __init__(self, txn_ctrl, handler, settle_before_discharge=False):
        self.txn_ctrl = txn_ctrl
        self.handler = handler
        self.id = None
        self._declare = None
        self._discharge = None
        self.failed = False
        self._pending = []
        self.settle_before_discharge = settle_before_discharge
        self.declare()

    def commit(self):
        self.discharge(False)

    def abort(self):
        self.discharge(True)

    def declare(self):
        self._declare = self._send_ctrl(symbol(u'amqp:declare:list'), [None])

    def discharge(self, failed):
        self.failed = failed
        self._discharge = self._send_ctrl(symbol(u'amqp:discharge:list'), [self.id, failed])

    def _send_ctrl(self, descriptor, value):
        delivery = self.txn_ctrl.send(Message(body=Described(descriptor, value)))
        delivery.transaction = self
        return delivery

    def send(self, sender, msg, tag=None):
        dlv = sender.send(msg, tag=tag)
        dlv.local.data = [self.id]
        dlv.update(0x34)
        return dlv

    def accept(self, delivery):
        self.update(delivery, PN_ACCEPTED)
        if self.settle_before_discharge:
            delivery.settle()
        else:
            self._pending.append(delivery)

    def update(self, delivery, state=None):
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

    def apply(self, link):
        """
        Subclasses will implement any configuration logic in this
        method
        """
        pass

    def test(self, link):
        """
        Subclasses can override this to selectively apply an option
        e.g. based on some link criteria
        """
        return True


class AtMostOnce(LinkOption):
    def apply(self, link):
        link.snd_settle_mode = Link.SND_SETTLED


class AtLeastOnce(LinkOption):
    def apply(self, link):
        link.snd_settle_mode = Link.SND_UNSETTLED
        link.rcv_settle_mode = Link.RCV_FIRST


class SenderOption(LinkOption):
    def apply(self, sender): pass

    def test(self, link): return link.is_sender


class ReceiverOption(LinkOption):
    def apply(self, receiver): pass

    def test(self, link): return link.is_receiver


class DynamicNodeProperties(LinkOption):
    def __init__(self, props={}):
        self.properties = {}
        for k in props:
            if isinstance(k, symbol):
                self.properties[k] = props[k]
            else:
                self.properties[symbol(k)] = props[k]

    def apply(self, link):
        if link.is_receiver:
            link.source.properties.put_dict(self.properties)
        else:
            link.target.properties.put_dict(self.properties)


class Filter(ReceiverOption):
    def __init__(self, filter_set={}):
        self.filter_set = filter_set

    def apply(self, receiver):
        receiver.source.filter.put_dict(self.filter_set)


class Selector(Filter):
    """
    Configures a link with a message selector filter
    """

    def __init__(self, value, name='selector'):
        super(Selector, self).__init__({symbol(name): Described(symbol('apache.org:selector-filter:string'), utf82unicode(value))})


class DurableSubscription(ReceiverOption):
    def apply(self, receiver):
        receiver.source.durability = Terminus.DELIVERIES
        receiver.source.expiry_policy = Terminus.EXPIRE_NEVER


class Move(ReceiverOption):
    def apply(self, receiver):
        receiver.source.distribution_mode = Terminus.DIST_MODE_MOVE


class Copy(ReceiverOption):
    def apply(self, receiver):
        receiver.source.distribution_mode = Terminus.DIST_MODE_COPY


def _apply_link_options(options, link):
    if options:
        if isinstance(options, list):
            for o in options:
                if o.test(link): o.apply(link)
        else:
            if options.test(link): options.apply(link)


def _create_session(connection, handler=None):
    session = connection.session()
    session.open()
    return session


def _get_attr(target, name):
    if hasattr(target, name):
        return getattr(target, name)
    else:
        return None


class SessionPerConnection(object):
    def __init__(self):
        self._default_session = None

    def session(self, connection):
        if not self._default_session:
            self._default_session = _create_session(connection)
        return self._default_session


class GlobalOverrides(Handler):
    """
    Internal handler that triggers the necessary socket connect for an
    opened connection.
    """

    def __init__(self, base):
        self.base = base

    def on_unhandled(self, name, event):
        if not self._override(event):
            event.dispatch(self.base)

    def _override(self, event):
        conn = event.connection
        return conn and hasattr(conn, '_overrides') and event.dispatch(conn._overrides)


class Acceptor(Handler):

    def __init__(self, reactor, host, port, handler=None):
        self._ssl_domain = None
        self._reactor = reactor
        self._handler = handler
        sock = IO.listen(host, port)
        s = reactor.selectable(handler=self, delegate=sock)
        s.reading = True
        s._transport = None
        self._selectable = s
        reactor.update(s)

    def set_ssl_domain(self, ssl_domain):
        self._ssl_domain = ssl_domain

    def close(self):
        if not self._selectable.is_terminal:
            IO.close(self._selectable)
            self._selectable.terminate()
            self._reactor.update(self._selectable)

    def on_selectable_readable(self, event):
        s = event.selectable

        sock, name = IO.accept(self._selectable)
        _logger.debug("Accepted connection from %s", name)

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

class Connector(Handler):
    """
    Internal handler that triggers the necessary socket connect for an
    opened connection.
    """

    def __init__(self, connection):
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

    def _connect(self, connection):
        url = self.address.next()
        connection.url = url
        # if virtual-host not set, use host from address as default
        if self.virtual_host is None:
            connection.hostname = url.host
        _logger.debug("connecting to %r..." % url)

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

    def on_connection_local_open(self, event):
        self._connect(event.connection)

    def on_connection_remote_open(self, event):
        _logger.debug("connected to %s" % event.connection.hostname)
        if self.reconnect:
            self.reconnect.reset()

    def on_transport_tail_closed(self, event):
        event.transport.close_head()

    def on_transport_closed(self, event):
        if self.connection is None: return
        if self.connection.state & Endpoint.LOCAL_ACTIVE:
            if self.reconnect:
                event.transport.unbind()
                delay = self.reconnect.next()
                if delay == 0:
                    _logger.info("Disconnected, reconnecting...")
                    self._connect(self.connection)
                    return
                else:
                    _logger.info("Disconnected will try to reconnect after %s seconds" % delay)
                    event.reactor.schedule(delay, self)
                    return
            else:
                _logger.debug("Disconnected")
        # See connector.cpp: conn.free()/pn_connection_release() here?
        self.connection = None

    def on_timer_task(self, event):
        self._connect(self.connection)


class Backoff(object):
    """
    A reconnect strategy involving an increasing delay between
    retries, up to a maximum or 10 seconds.
    """

    def __init__(self):
        self.delay = 0

    def reset(self):
        self.delay = 0

    def next(self):
        current = self.delay
        if current == 0:
            self.delay = 0.1
        else:
            self.delay = min(10, 2 * current)
        return current


class Urls(object):
    def __init__(self, values):
        self.values = [Url(v) for v in values]
        self.i = iter(self.values)

    def __iter__(self):
        return self

    def next(self):
        try:
            return next(self.i)
        except StopIteration:
            self.i = iter(self.values)
            return next(self.i)


class SSLConfig(object):
    def __init__(self):
        self.client = SSLDomain(SSLDomain.MODE_CLIENT)
        self.server = SSLDomain(SSLDomain.MODE_SERVER)

    def set_credentials(self, cert_file, key_file, password):
        self.client.set_credentials(cert_file, key_file, password)
        self.server.set_credentials(cert_file, key_file, password)

    def set_trusted_ca_db(self, certificate_db):
        self.client.set_trusted_ca_db(certificate_db)
        self.server.set_trusted_ca_db(certificate_db)

def _find_config_file():
    confname = 'connect.json'
    confpath = ['.', '~/.config/messaging','/etc/messaging']
    for d in confpath:
        f = os.path.join(d, confname)
        if os.path.isfile(f):
            return f
    return None

def _get_default_config():
    conf = os.environ.get('MESSAGING_CONNECT_FILE') or _find_config_file()
    if conf and os.path.isfile(conf):
        with open(conf, 'r') as f:
            json_text = f.read()
            json_text = _strip_json_comments(json_text)
            return json.loads(json_text)
    else:
        return {}

def _strip_json_comments(json_text):
    """This strips c-style comments from text, taking into account '/*comments*/' and '//comments'
    nested inside a string etc."""
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " " # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"', re.DOTALL | re.MULTILINE)
    return re.sub(pattern, replacer, json_text)

def _get_default_port_for_scheme(scheme):
    if scheme == 'amqps':
        return 5671
    else:
        return 5672

class Container(Reactor):
    """A representation of the AMQP concept of a 'container', which
       loosely speaking is something that establishes links to or from
       another container, over which messages are transfered. This is
       an extension to the Reactor class that adds convenience methods
       for creating connections and sender- or receiver- links.
    """

    def __init__(self, *handlers, **kwargs):
        super(Container, self).__init__(*handlers, **kwargs)
        if "impl" not in kwargs:
            try:
                self.ssl = SSLConfig()
            except SSLUnavailable:
                self.ssl = None
            self.global_handler = GlobalOverrides(kwargs.get('global_handler', self.global_handler))
            self.trigger = None
            self.container_id = str(_generate_uuid())
            self.allow_insecure_mechs = True
            self.allowed_mechs = None
            self.sasl_enabled = True
            self.user = None
            self.password = None

    def connect(self, url=None, urls=None, address=None, handler=None, reconnect=None, heartbeat=None, ssl_domain=None,
                **kwargs):
        """
        Initiates the establishment of an AMQP connection. Returns an
        instance of proton.Connection.

        @param url: URL string of process to connect to

        @param urls: list of URL strings of process to try to connect to

        Only one of url or urls should be specified.

        @param reconnect: Reconnect is enabled by default.  You can
        pass in an instance of Backoff to control reconnect behavior.
        A value of False will prevent the library from automatically
        trying to reconnect if the underlying socket is disconnected
        before the connection has been closed.

        @param heartbeat: A value in milliseconds indicating the
        desired frequency of heartbeats used to test the underlying
        socket is alive.

        @param ssl_domain: SSL configuration in the form of an
        instance of proton.SSLDomain.

        @param handler: a connection scoped handler that will be
        called to process any events in the scope of this connection
        or its child links

        @param kwargs: 'sasl_enabled', which determines whether a sasl
        layer is used for the connection. 'allowed_mechs', an optional
        string specifying the SASL mechanisms allowed for this
        connection; the value is a space-separated list of mechanism
        names; the mechanisms allowed by default are determined by
        your SASL library and system configuration, with two
        exceptions: GSSAPI and GSS-SPNEGO are disabled by default; to
        enable them, you must explicitly add them using this option;
        clients must set the allowed mechanisms before the the
        outgoing connection is attempted; servers must set them before
        the listening connection is setup.  'allow_insecure_mechs', a
        flag indicating whether insecure mechanisms, such as PLAIN
        over a non-encrypted socket, are allowed. 'virtual_host', the
        hostname to set in the Open performative used by peer to
        determine the correct back-end service for the client; if
        'virtual_host' is not supplied the host field from the URL is
        used instead. 'user', the user to authenticate. 'password',
        the authentication secret.

        """
        if not url and not urls and not address:
            config = _get_default_config()
            scheme = config.get('scheme', 'amqp')
            _url = "%s://%s:%s" % (scheme, config.get('host', 'localhost'), config.get('port', _get_default_port_for_scheme(scheme)))
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
                if ca:
                    _ssl_domain.set_trusted_ca_db(str(ca))
                    if tls_config.get('verify', True):
                        _ssl_domain.set_peer_authentication(SSLDomain.VERIFY_PEER_NAME, str(ca))
                if cert and key:
                    _ssl_domain.set_credentials(str(cert), str(key), None)

            return self._connect(_url, handler=handler, reconnect=reconnect, heartbeat=heartbeat, ssl_domain=_ssl_domain, **_kwargs)
        else:
            return self._connect(url=url, urls=urls, handler=handler, reconnect=reconnect, heartbeat=heartbeat, ssl_domain=ssl_domain, **kwargs)

    def _connect(self, url=None, urls=None, address=None, handler=None, reconnect=None, heartbeat=None, ssl_domain=None, **kwargs):
        conn = self.connection(handler)
        conn.container = self.container_id or str(_generate_uuid())
        conn.offered_capabilities = kwargs.get('offered_capabilities')
        conn.desired_capabilities = kwargs.get('desired_capabilities')
        conn.properties = kwargs.get('properties')

        connector = Connector(conn)
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
        elif address:
            connector.address = address
        else:
            raise ValueError("One of url, urls or address required")
        if heartbeat:
            connector.heartbeat = heartbeat
        if reconnect:
            connector.reconnect = reconnect
        elif reconnect is None:
            connector.reconnect = Backoff()
        # use container's default client domain if none specified.  This is
        # only necessary of the URL specifies the "amqps:" scheme
        connector.ssl_domain = ssl_domain or (self.ssl and self.ssl.client)
        conn._session_policy = SessionPerConnection()  # todo: make configurable
        conn.open()
        return conn

    def _get_id(self, container, remote, local):
        if local and remote:
            "%s-%s-%s" % (container, remote, local)
        elif local:
            return "%s-%s" % (container, local)
        elif remote:
            return "%s-%s" % (container, remote)
        else:
            return "%s-%s" % (container, str(_generate_uuid()))

    def _get_session(self, context):
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

    def create_sender(self, context, target=None, source=None, name=None, handler=None, tags=None, options=None):
        """
        Initiates the establishment of a link over which messages can
        be sent. Returns an instance of proton.Sender.

        There are two patterns of use. (1) A connection can be passed
        as the first argument, in which case the link is established
        on that connection. In this case the target address can be
        specified as the second argument (or as a keyword
        argument). The source address can also be specified if
        desired. (2) Alternatively a URL can be passed as the first
        argument. In this case a new connection will be established on
        which the link will be attached. If a path is specified and
        the target is not, then the path of the URL is used as the
        target address.

        The name of the link may be specified if desired, otherwise a
        unique name will be generated.

        Various LinkOptions can be specified to further control the
        attachment.
        """
        if isstring(context):
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

    def create_receiver(self, context, source=None, target=None, name=None, dynamic=False, handler=None, options=None):
        """
        Initiates the establishment of a link over which messages can
        be received (aka a subscription). Returns an instance of
        proton.Receiver.

        There are two patterns of use. (1) A connection can be passed
        as the first argument, in which case the link is established
        on that connection. In this case the source address can be
        specified as the second argument (or as a keyword
        argument). The target address can also be specified if
        desired. (2) Alternatively a URL can be passed as the first
        argument. In this case a new connection will be established on
        which the link will be attached. If a path is specified and
        the source is not, then the path of the URL is used as the
        target address.

        The name of the link may be specified if desired, otherwise a
        unique name will be generated.

        Various LinkOptions can be specified to further control the
        attachment.
        """
        if isstring(context):
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

    def declare_transaction(self, context, handler=None, settle_before_discharge=False):
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

    def listen(self, url, ssl_domain=None):
        """
        Initiates a server socket, accepting incoming AMQP connections
        on the interface and port specified.
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

    def do_work(self, timeout=None):
        if timeout:
            self.timeout = timeout
        return self.process()
