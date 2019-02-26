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

import json
import re
import os
import logging
import traceback
import uuid

from cproton import PN_MILLIS_MAX, PN_PYREF, PN_ACCEPTED, \
    pn_reactor_stop, pn_selectable_attachments, pn_reactor_quiesced, pn_reactor_acceptor, \
    pn_record_set_handler, pn_collector_put, pn_reactor_get_timeout, pn_task_cancel, pn_acceptor_set_ssl_domain, \
    pn_record_get, pn_reactor_selectable, pn_task_attachments, pn_reactor_schedule, pn_acceptor_close, pn_py2void, \
    pn_reactor_error, pn_reactor_attachments, pn_reactor_get_global_handler, pn_reactor_process, pn_reactor, \
    pn_reactor_set_handler, pn_reactor_set_global_handler, pn_reactor_yield, pn_error_text, pn_reactor_connection, \
    pn_cast_pn_reactor, pn_reactor_get_connection_address, pn_reactor_update, pn_reactor_collector, pn_void2py, \
    pn_reactor_start, pn_reactor_set_connection_host, pn_cast_pn_task, pn_decref, pn_reactor_set_timeout, \
    pn_reactor_mark, pn_reactor_get_handler, pn_reactor_wakeup

from ._delivery import  Delivery
from ._endpoints import Connection, Endpoint, Link, Session, Terminus
from ._exceptions import SSLUnavailable
from ._data import Described, symbol, ulong
from ._message import  Message
from ._transport import Transport, SSL, SSLDomain
from ._url import Url
from ._common import isstring, secs2millis, millis2secs, unicode2utf8, utf82unicode
from ._events import EventType, EventBase, Handler
from ._reactor_impl import Selectable, WrappedHandler, _chandler
from ._wrapper import Wrapper, PYCTX

from ._handlers import OutgoingMessageHandler

from . import _compat
from ._compat import queue

Logger = logging.getLogger("proton")


def _generate_uuid():
    return uuid.uuid4()


def _timeout2millis(secs):
    if secs is None: return PN_MILLIS_MAX
    return secs2millis(secs)


def _millis2timeout(millis):
    if millis == PN_MILLIS_MAX: return None
    return millis2secs(millis)


class Task(Wrapper):

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None
        else:
            return Task(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_task_attachments)

    def _init(self):
        pass

    def cancel(self):
        pn_task_cancel(self._impl)


class Acceptor(Wrapper):

    def __init__(self, impl):
        Wrapper.__init__(self, impl)

    def set_ssl_domain(self, ssl_domain):
        pn_acceptor_set_ssl_domain(self._impl, ssl_domain._domain)

    def close(self):
        pn_acceptor_close(self._impl)


class Reactor(Wrapper):

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None
        else:
            record = pn_reactor_attachments(impl)
            attrs = pn_void2py(pn_record_get(record, PYCTX))
            if attrs and 'subclass' in attrs:
                return attrs['subclass'](impl=impl)
            else:
                return Reactor(impl=impl)

    def __init__(self, *handlers, **kwargs):
        Wrapper.__init__(self, kwargs.get("impl", pn_reactor), pn_reactor_attachments)
        for h in handlers:
            self.handler.add(h, on_error=self.on_error_delegate())

    def _init(self):
        self.errors = []

    # on_error relay handler tied to underlying C reactor.  Use when the
    # error will always be generated from a callback from this reactor.
    # Needed to prevent reference cycles and be compatible with wrappers.
    class ErrorDelegate(object):
        def __init__(self, reactor):
            self.reactor_impl = reactor._impl

        def on_error(self, info):
            ractor = Reactor.wrap(self.reactor_impl)
            ractor.on_error(info)

    def on_error_delegate(self):
        return Reactor.ErrorDelegate(self).on_error

    def on_error(self, info):
        self.errors.append(info)
        self.yield_()

    def _get_global(self):
        return WrappedHandler.wrap(pn_reactor_get_global_handler(self._impl), self.on_error_delegate())

    def _set_global(self, handler):
        impl = _chandler(handler, self.on_error_delegate())
        pn_reactor_set_global_handler(self._impl, impl)
        pn_decref(impl)

    global_handler = property(_get_global, _set_global)

    def _get_timeout(self):
        return _millis2timeout(pn_reactor_get_timeout(self._impl))

    def _set_timeout(self, secs):
        return pn_reactor_set_timeout(self._impl, _timeout2millis(secs))

    timeout = property(_get_timeout, _set_timeout)

    def yield_(self):
        pn_reactor_yield(self._impl)

    def mark(self):
        return pn_reactor_mark(self._impl)

    def _get_handler(self):
        return WrappedHandler.wrap(pn_reactor_get_handler(self._impl), self.on_error_delegate())

    def _set_handler(self, handler):
        impl = _chandler(handler, self.on_error_delegate())
        pn_reactor_set_handler(self._impl, impl)
        pn_decref(impl)

    handler = property(_get_handler, _set_handler)

    def run(self):
        self.timeout = 3.14159265359
        self.start()
        while self.process(): pass
        self.stop()
        self.process()
        self.global_handler = None
        self.handler = None

    def wakeup(self):
        n = pn_reactor_wakeup(self._impl)
        if n: raise IOError(pn_error_text(pn_reactor_error(self._impl)))

    def start(self):
        pn_reactor_start(self._impl)

    @property
    def quiesced(self):
        return pn_reactor_quiesced(self._impl)

    def _check_errors(self):
        if self.errors:
            for exc, value, tb in self.errors[:-1]:
                traceback.print_exception(exc, value, tb)
            exc, value, tb = self.errors[-1]
            _compat.raise_(exc, value, tb)

    def process(self):
        result = pn_reactor_process(self._impl)
        self._check_errors()
        return result

    def stop(self):
        pn_reactor_stop(self._impl)
        self._check_errors()

    def schedule(self, delay, task):
        impl = _chandler(task, self.on_error_delegate())
        task = Task.wrap(pn_reactor_schedule(self._impl, secs2millis(delay), impl))
        pn_decref(impl)
        return task

    def acceptor(self, host, port, handler=None):
        impl = _chandler(handler, self.on_error_delegate())
        aimpl = pn_reactor_acceptor(self._impl, unicode2utf8(host), str(port), impl)
        pn_decref(impl)
        if aimpl:
            return Acceptor(aimpl)
        else:
            raise IOError("%s (%s:%s)" % (pn_error_text(pn_reactor_error(self._impl)), host, port))

    def connection(self, handler=None):
        """Deprecated: use connection_to_host() instead
        """
        impl = _chandler(handler, self.on_error_delegate())
        result = Connection.wrap(pn_reactor_connection(self._impl, impl))
        if impl: pn_decref(impl)
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
        pn_reactor_set_connection_host(self._impl,
                                       connection._impl,
                                       unicode2utf8(str(host)),
                                       unicode2utf8(str(port)))

    def get_connection_address(self, connection):
        """This may be used to retrieve the remote peer address.
        @return: string containing the address in URL format or None if no
        address is available.  Use the proton.Url class to create a Url object
        from the returned value.
        """
        _url = pn_reactor_get_connection_address(self._impl, connection._impl)
        return utf82unicode(_url)

    def selectable(self, handler=None):
        impl = _chandler(handler, self.on_error_delegate())
        result = Selectable.wrap(pn_reactor_selectable(self._impl))
        if impl:
            record = pn_selectable_attachments(result._impl)
            pn_record_set_handler(record, impl)
            pn_decref(impl)
        return result

    def update(self, sel):
        pn_reactor_update(self._impl, sel._impl)

    def push_event(self, obj, etype):
        pn_collector_put(pn_reactor_collector(self._impl), PN_PYREF, pn_py2void(obj), etype.number)


from ._events import wrappers as _wrappers

_wrappers["pn_reactor"] = lambda x: Reactor.wrap(pn_cast_pn_reactor(x))
_wrappers["pn_task"] = lambda x: Task.wrap(pn_cast_pn_task(x))


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
        sel.fileno(self.fileno())
        sel.reading = True
        event.reactor.update(sel)

    def on_selectable_readable(self, event):
        os.read(self.pipe[0], 512)
        while not self.queue.empty():
            requested = self.queue.get()
            event.reactor.push_event(requested.context, requested.type)
        if self._closed:
            s = event.context
            s.terminate()
            event.reactor.update(s)


class ApplicationEvent(EventBase):
    """
    Application defined event, which can optionally be associated with
    an engine object and or an arbitrary subject
    """

    def __init__(self, typename, connection=None, session=None, link=None, delivery=None, subject=None):
        super(ApplicationEvent, self).__init__(PN_PYREF, self, EventType(typename))
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
                Logger.warning("Unexpected outcome for declare: %s" % event.delivery.remote_state)
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
        super(Selector, self).__init__({symbol(name): Described(symbol('apache.org:selector-filter:string'), value)})


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


class GlobalOverrides(object):
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

    def _connect(self, connection, reactor):
        assert (reactor is not None)
        url = self.address.next()
        reactor.set_connection_host(connection, url.host, str(url.port))
        # if virtual-host not set, use host from address as default
        if self.virtual_host is None:
            connection.hostname = url.host
        Logger.debug("connecting to %r..." % url)

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
        self._connect(event.connection, event.reactor)

    def on_connection_remote_open(self, event):
        Logger.debug("connected to %s" % event.connection.hostname)
        if self.reconnect:
            self.reconnect.reset()
            self.transport = None

    def on_transport_tail_closed(self, event):
        self.on_transport_closed(event)

    def on_transport_closed(self, event):
        if self.connection is None: return
        if self.connection.state & Endpoint.LOCAL_ACTIVE:
            if self.reconnect:
                event.transport.unbind()
                delay = self.reconnect.next()
                if delay == 0:
                    Logger.info("Disconnected, reconnecting...")
                    self._connect(self.connection, event.reactor)
                    return
                else:
                    Logger.info("Disconnected will try to reconnect after %s seconds" % delay)
                    event.reactor.schedule(delay, self)
                    return
            else:
                Logger.debug("Disconnected")
        # See connector.cpp: conn.free()/pn_connection_release() here?
        self.connection = None

    def on_timer_task(self, event):
        self._connect(self.connection, event.reactor)


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

def find_config_file():
    confname = 'connect.json'
    confpath = ['.', '~/.config/messaging','/etc/messaging']
    for d in confpath:
        f = os.path.join(d, confname)
        if os.path.isfile(f):
            return f
    return None

def get_default_config():
    conf = os.environ.get('MESSAGING_CONNECT_FILE') or find_config_file()
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

def get_default_port_for_scheme(scheme):
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
            Wrapper.__setattr__(self, 'subclass', self.__class__)

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
            config = get_default_config()
            scheme = config.get('scheme', 'amqp')
            _url = "%s://%s:%s" % (scheme, config.get('host', 'localhost'), config.get('port', get_default_port_for_scheme(scheme)))
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
        if handler != None:
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
        if handler != None:
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
