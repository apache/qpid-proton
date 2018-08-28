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

import threading

from cproton import PN_SESSION_REMOTE_CLOSE, PN_SESSION_FINAL, pn_event_context, pn_collector_put, \
    PN_SELECTABLE_UPDATED, pn_collector, PN_CONNECTION_REMOTE_OPEN, pn_event_attachments, pn_event_type, \
    pn_collector_free, pn_handler_dispatch, PN_SELECTABLE_WRITABLE, PN_SELECTABLE_INIT, PN_SESSION_REMOTE_OPEN, \
    pn_collector_peek, PN_CONNECTION_BOUND, PN_LINK_FLOW, pn_event_connection, PN_LINK_LOCAL_CLOSE, \
    PN_TRANSPORT_ERROR, PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_LOCAL_CLOSE, pn_event_delivery, \
    PN_LINK_REMOTE_OPEN, PN_TRANSPORT_CLOSED, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT, pn_event_reactor, \
    PN_CONNECTION_REMOTE_CLOSE, pn_collector_pop, PN_LINK_INIT, pn_event_link, PN_CONNECTION_UNBOUND, \
    pn_event_type_name, pn_event_session, PN_LINK_FINAL, pn_py2void, PN_REACTOR_INIT, PN_REACTOR_QUIESCED, \
    PN_LINK_LOCAL_DETACH, PN_SESSION_INIT, PN_CONNECTION_FINAL, PN_TIMER_TASK, pn_class_name, PN_SELECTABLE_READABLE, \
    pn_event_transport, PN_TRANSPORT_TAIL_CLOSED, PN_SELECTABLE_FINAL, PN_SESSION_LOCAL_OPEN, PN_DELIVERY, \
    PN_SESSION_LOCAL_CLOSE, pn_event_copy, PN_REACTOR_FINAL, PN_LINK_LOCAL_OPEN, PN_SELECTABLE_EXPIRED, \
    PN_LINK_REMOTE_DETACH, PN_PYREF, PN_LINK_REMOTE_CLOSE, pn_event_root, PN_SELECTABLE_ERROR, \
    PN_CONNECTION_INIT, pn_event_class, pn_void2py, pn_cast_pn_session, pn_cast_pn_link, pn_cast_pn_delivery, \
    pn_cast_pn_transport, pn_cast_pn_connection, pn_cast_pn_selectable

from ._common import Constant
from ._delivery import Delivery
from ._endpoints import Connection, Session, Link
from ._reactor_impl import Selectable, WrappedHandler
from ._transport import Transport
from ._wrapper import Wrapper


class Collector:

    def __init__(self):
        self._impl = pn_collector()

    def put(self, obj, etype):
        pn_collector_put(self._impl, PN_PYREF, pn_py2void(obj), etype.number)

    def peek(self):
        return Event.wrap(pn_collector_peek(self._impl))

    def pop(self):
        ev = self.peek()
        pn_collector_pop(self._impl)

    def __del__(self):
        pn_collector_free(self._impl)
        del self._impl


if "TypeExtender" not in globals():
    class TypeExtender:
        def __init__(self, number):
            self.number = number

        def next(self):
            try:
                return self.number
            finally:
                self.number += 1


class EventType(object):
    _lock = threading.Lock()
    _extended = TypeExtender(10000)
    TYPES = {}

    def __init__(self, name=None, number=None, method=None):
        if name is None and number is None:
            raise TypeError("extended events require a name")
        try:
            self._lock.acquire()
            if name is None:
                name = pn_event_type_name(number)

            if number is None:
                number = self._extended.next()

            if method is None:
                method = "on_%s" % name

            self.name = name
            self.number = number
            self.method = method

            self.TYPES[number] = self
        finally:
            self._lock.release()

    def __repr__(self):
        return self.name


def _dispatch(handler, method, *args):
    m = getattr(handler, method, None)
    if m:
        return m(*args)
    elif hasattr(handler, "on_unhandled"):
        return handler.on_unhandled(method, *args)


class EventBase(object):

    def __init__(self, clazz, context, type):
        self.clazz = clazz
        self.context = context
        self.type = type

    def dispatch(self, handler):
        return _dispatch(handler, self.type.method, self)


def _none(x): return None


DELEGATED = Constant("DELEGATED")


def _core(number, method):
    return EventType(number=number, method=method)


wrappers = {
    "pn_void": lambda x: pn_void2py(x),
    "pn_pyref": lambda x: pn_void2py(x),
    "pn_connection": lambda x: Connection.wrap(pn_cast_pn_connection(x)),
    "pn_session": lambda x: Session.wrap(pn_cast_pn_session(x)),
    "pn_link": lambda x: Link.wrap(pn_cast_pn_link(x)),
    "pn_delivery": lambda x: Delivery.wrap(pn_cast_pn_delivery(x)),
    "pn_transport": lambda x: Transport.wrap(pn_cast_pn_transport(x)),
    "pn_selectable": lambda x: Selectable.wrap(pn_cast_pn_selectable(x))
}


class Event(Wrapper, EventBase):
    REACTOR_INIT = _core(PN_REACTOR_INIT, "on_reactor_init")
    REACTOR_QUIESCED = _core(PN_REACTOR_QUIESCED, "on_reactor_quiesced")
    REACTOR_FINAL = _core(PN_REACTOR_FINAL, "on_reactor_final")

    TIMER_TASK = _core(PN_TIMER_TASK, "on_timer_task")

    CONNECTION_INIT = _core(PN_CONNECTION_INIT, "on_connection_init")
    CONNECTION_BOUND = _core(PN_CONNECTION_BOUND, "on_connection_bound")
    CONNECTION_UNBOUND = _core(PN_CONNECTION_UNBOUND, "on_connection_unbound")
    CONNECTION_LOCAL_OPEN = _core(PN_CONNECTION_LOCAL_OPEN, "on_connection_local_open")
    CONNECTION_LOCAL_CLOSE = _core(PN_CONNECTION_LOCAL_CLOSE, "on_connection_local_close")
    CONNECTION_REMOTE_OPEN = _core(PN_CONNECTION_REMOTE_OPEN, "on_connection_remote_open")
    CONNECTION_REMOTE_CLOSE = _core(PN_CONNECTION_REMOTE_CLOSE, "on_connection_remote_close")
    CONNECTION_FINAL = _core(PN_CONNECTION_FINAL, "on_connection_final")

    SESSION_INIT = _core(PN_SESSION_INIT, "on_session_init")
    SESSION_LOCAL_OPEN = _core(PN_SESSION_LOCAL_OPEN, "on_session_local_open")
    SESSION_LOCAL_CLOSE = _core(PN_SESSION_LOCAL_CLOSE, "on_session_local_close")
    SESSION_REMOTE_OPEN = _core(PN_SESSION_REMOTE_OPEN, "on_session_remote_open")
    SESSION_REMOTE_CLOSE = _core(PN_SESSION_REMOTE_CLOSE, "on_session_remote_close")
    SESSION_FINAL = _core(PN_SESSION_FINAL, "on_session_final")

    LINK_INIT = _core(PN_LINK_INIT, "on_link_init")
    LINK_LOCAL_OPEN = _core(PN_LINK_LOCAL_OPEN, "on_link_local_open")
    LINK_LOCAL_CLOSE = _core(PN_LINK_LOCAL_CLOSE, "on_link_local_close")
    LINK_LOCAL_DETACH = _core(PN_LINK_LOCAL_DETACH, "on_link_local_detach")
    LINK_REMOTE_OPEN = _core(PN_LINK_REMOTE_OPEN, "on_link_remote_open")
    LINK_REMOTE_CLOSE = _core(PN_LINK_REMOTE_CLOSE, "on_link_remote_close")
    LINK_REMOTE_DETACH = _core(PN_LINK_REMOTE_DETACH, "on_link_remote_detach")
    LINK_FLOW = _core(PN_LINK_FLOW, "on_link_flow")
    LINK_FINAL = _core(PN_LINK_FINAL, "on_link_final")

    DELIVERY = _core(PN_DELIVERY, "on_delivery")

    TRANSPORT = _core(PN_TRANSPORT, "on_transport")
    TRANSPORT_ERROR = _core(PN_TRANSPORT_ERROR, "on_transport_error")
    TRANSPORT_HEAD_CLOSED = _core(PN_TRANSPORT_HEAD_CLOSED, "on_transport_head_closed")
    TRANSPORT_TAIL_CLOSED = _core(PN_TRANSPORT_TAIL_CLOSED, "on_transport_tail_closed")
    TRANSPORT_CLOSED = _core(PN_TRANSPORT_CLOSED, "on_transport_closed")

    SELECTABLE_INIT = _core(PN_SELECTABLE_INIT, "on_selectable_init")
    SELECTABLE_UPDATED = _core(PN_SELECTABLE_UPDATED, "on_selectable_updated")
    SELECTABLE_READABLE = _core(PN_SELECTABLE_READABLE, "on_selectable_readable")
    SELECTABLE_WRITABLE = _core(PN_SELECTABLE_WRITABLE, "on_selectable_writable")
    SELECTABLE_EXPIRED = _core(PN_SELECTABLE_EXPIRED, "on_selectable_expired")
    SELECTABLE_ERROR = _core(PN_SELECTABLE_ERROR, "on_selectable_error")
    SELECTABLE_FINAL = _core(PN_SELECTABLE_FINAL, "on_selectable_final")

    @staticmethod
    def wrap(impl, number=None):
        if impl is None:
            return None

        if number is None:
            number = pn_event_type(impl)

        event = Event(impl, number)

        # check for an application defined ApplicationEvent and return that.  This
        # avoids an expensive wrap operation invoked by event.context
        if pn_event_class(impl) == PN_PYREF and \
                isinstance(event.context, EventBase):
            return event.context
        else:
            return event

    def __init__(self, impl, number):
        Wrapper.__init__(self, impl, pn_event_attachments)
        self.__dict__["type"] = EventType.TYPES[number]

    def _init(self):
        pass

    def copy(self):
        copy = pn_event_copy(self._impl)
        return Event.wrap(copy)

    @property
    def clazz(self):
        cls = pn_event_class(self._impl)
        if cls:
            return pn_class_name(cls)
        else:
            return None

    @property
    def root(self):
        return WrappedHandler.wrap(pn_event_root(self._impl))

    @property
    def context(self):
        """Returns the context object associated with the event. The type of this depend on the type of event."""
        return wrappers[self.clazz](pn_event_context(self._impl))

    def dispatch(self, handler, type=None):
        type = type or self.type
        if isinstance(handler, WrappedHandler):
            pn_handler_dispatch(handler._impl, self._impl, type.number)
        else:
            result = _dispatch(handler, type.method, self)
            if result != DELEGATED and hasattr(handler, "handlers"):
                for h in handler.handlers:
                    self.dispatch(h, type)

    @property
    def reactor(self):
        """Returns the reactor associated with the event."""
        return wrappers.get("pn_reactor", _none)(pn_event_reactor(self._impl))

    def __getattr__(self, name):
        r = self.reactor
        if r and hasattr(r, 'subclass') and r.subclass.__name__.lower() == name:
            return r
        else:
            return super(Event, self).__getattr__(name)

    @property
    def transport(self):
        """Returns the transport associated with the event, or null if none is associated with it."""
        return Transport.wrap(pn_event_transport(self._impl))

    @property
    def connection(self):
        """Returns the connection associated with the event, or null if none is associated with it."""
        return Connection.wrap(pn_event_connection(self._impl))

    @property
    def session(self):
        """Returns the session associated with the event, or null if none is associated with it."""
        return Session.wrap(pn_event_session(self._impl))

    @property
    def link(self):
        """Returns the link associated with the event, or null if none is associated with it."""
        return Link.wrap(pn_event_link(self._impl))

    @property
    def sender(self):
        """Returns the sender link associated with the event, or null if
           none is associated with it. This is essentially an alias for
           link(), that does an additional checkon the type of the
           link."""
        l = self.link
        if l and l.is_sender:
            return l
        else:
            return None

    @property
    def receiver(self):
        """Returns the receiver link associated with the event, or null if
           none is associated with it. This is essentially an alias for
           link(), that does an additional checkon the type of the link."""
        l = self.link
        if l and l.is_receiver:
            return l
        else:
            return None

    @property
    def delivery(self):
        """Returns the delivery associated with the event, or null if none is associated with it."""
        return Delivery.wrap(pn_event_delivery(self._impl))

    def __repr__(self):
        return "%s(%s)" % (self.type, self.context)


class LazyHandlers(object):
    def __get__(self, obj, clazz):
        if obj is None:
            return self
        ret = []
        obj.__dict__['handlers'] = ret
        return ret


class Handler(object):
    handlers = LazyHandlers()

    def on_unhandled(self, method, *args):
        pass
