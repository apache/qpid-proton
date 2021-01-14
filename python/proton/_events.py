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

from cproton import PN_CONNECTION_BOUND, PN_CONNECTION_FINAL, PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_CLOSE, \
    PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_REMOTE_CLOSE, PN_CONNECTION_REMOTE_OPEN, PN_CONNECTION_UNBOUND, PN_DELIVERY, \
    PN_LINK_FINAL, PN_LINK_FLOW, PN_LINK_INIT, PN_LINK_LOCAL_CLOSE, PN_LINK_LOCAL_DETACH, PN_LINK_LOCAL_OPEN, \
    PN_LINK_REMOTE_CLOSE, PN_LINK_REMOTE_DETACH, PN_LINK_REMOTE_OPEN, PN_PYREF, PN_SESSION_FINAL, PN_SESSION_INIT, \
    PN_SESSION_LOCAL_CLOSE, PN_SESSION_LOCAL_OPEN, PN_SESSION_REMOTE_CLOSE, PN_SESSION_REMOTE_OPEN, PN_TIMER_TASK, \
    PN_TRANSPORT, PN_TRANSPORT_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_TAIL_CLOSED, \
    pn_cast_pn_connection, pn_cast_pn_delivery, pn_cast_pn_link, pn_cast_pn_session, pn_cast_pn_transport, \
    pn_class_name, pn_collector, pn_collector_free, pn_collector_more, pn_collector_peek, pn_collector_pop, \
    pn_collector_put, pn_collector_release, pn_event_class, pn_event_connection, pn_event_context, pn_event_delivery, \
    pn_event_link, pn_event_session, pn_event_transport, pn_event_type, pn_event_type_name, pn_py2void, pn_void2py

from ._delivery import Delivery
from ._endpoints import Connection, Link, Session
from ._transport import Transport


class Collector:

    def __init__(self):
        self._impl = pn_collector()

    def put(self, obj, etype):
        pn_collector_put(self._impl, PN_PYREF, pn_py2void(obj), etype.number)

    def peek(self):
        return Event.wrap(pn_collector_peek(self._impl))

    def more(self):
        return pn_collector_more(self._impl)

    def pop(self):
        ev = self.peek()
        pn_collector_pop(self._impl)

    def release(self):
        pn_collector_release(self._impl)

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
    """
    Connects an event number to an event name, and is used
    internally by :class:`Event` to represent all known
    event types. A global list of events is maintained. An
    :class:`EventType` created with a name but no number is
    treated as an *extended* event, and is assigned an
    internal event number starting at 10000.
    """
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
        return "EventType(name=%s, number=%d)" % (self.name, self.number)

    def __str__(self):
        return self.name


def _dispatch(handler, method, *args):
    m = getattr(handler, method, None)
    if m:
        m(*args)
    elif hasattr(handler, "on_unhandled"):
        handler.on_unhandled(method, *args)


class EventBase(object):

    def __init__(self, type):
        self._type = type

    @property
    def type(self):
        """
        The type name for this event

        :type: ``str``
        """
        return self._type

    @property
    def handler(self):
        """
        The handler for this event type. Not implemented, always returns ``None``.

        :type: ``None``
        """
        return None

    def dispatch(self, handler, type=None):
        """
        Process this event by sending it to all known handlers that
        are valid for this event type.

        :param handler: Parent handler to process this event
        :type handler: :class:`Handler`
        :param type: Event type
        :type type: :class:`EventType`
        """
        type = type or self._type
        _dispatch(handler, type.method, self)
        if hasattr(handler, "handlers"):
            for h in handler.handlers:
                self.dispatch(h, type)

    def __repr__(self):
        return "%s(%r)" % (self._type, self.context)


def _core(number, method):
    return EventType(number=number, method=method)


def _internal(name):
    return EventType(name=name)


wrappers = {
    "pn_void": lambda x: pn_void2py(x),
    "pn_pyref": lambda x: pn_void2py(x),
    "pn_connection": lambda x: Connection.wrap(pn_cast_pn_connection(x)),
    "pn_session": lambda x: Session.wrap(pn_cast_pn_session(x)),
    "pn_link": lambda x: Link.wrap(pn_cast_pn_link(x)),
    "pn_delivery": lambda x: Delivery.wrap(pn_cast_pn_delivery(x)),
    "pn_transport": lambda x: Transport.wrap(pn_cast_pn_transport(x))
}


class Event(EventBase):
    """
    Notification of a state change in the protocol engine.
    """
    TIMER_TASK = _core(PN_TIMER_TASK, "on_timer_task")
    """A timer event has occurred."""

    CONNECTION_INIT = _core(PN_CONNECTION_INIT, "on_connection_init")
    """
    The connection has been created. This is the first event that
    will ever be issued for a connection. Events of this type point
    to the relevant connection.
    """

    CONNECTION_BOUND = _core(PN_CONNECTION_BOUND, "on_connection_bound")
    """
    The connection has been bound to a transport. This event is
    issued when the :meth:`Transport.bind` operation is invoked.
    """

    CONNECTION_UNBOUND = _core(PN_CONNECTION_UNBOUND, "on_connection_unbound")
    """
    The connection has been unbound from its transport. This event is
    issued when the :meth:`Transport.unbind` operation is invoked.
    """

    CONNECTION_LOCAL_OPEN = _core(PN_CONNECTION_LOCAL_OPEN, "on_connection_local_open")
    """
    The local connection endpoint has been closed. Events of this
    type point to the relevant connection.
    """

    CONNECTION_LOCAL_CLOSE = _core(PN_CONNECTION_LOCAL_CLOSE, "on_connection_local_close")
    """
    The local connection endpoint has been closed. Events of this
    type point to the relevant connection.
    """

    CONNECTION_REMOTE_OPEN = _core(PN_CONNECTION_REMOTE_OPEN, "on_connection_remote_open")
    """
    The remote endpoint has opened the connection. Events of this
    type point to the relevant connection.
    """

    CONNECTION_REMOTE_CLOSE = _core(PN_CONNECTION_REMOTE_CLOSE, "on_connection_remote_close")
    """
    The remote endpoint has closed the connection. Events of this
    type point to the relevant connection.
    """

    CONNECTION_FINAL = _core(PN_CONNECTION_FINAL, "on_connection_final")
    """
    The connection has been freed and any outstanding processing has
    been completed. This is the final event that will ever be issued
    for a connection.
    """

    SESSION_INIT = _core(PN_SESSION_INIT, "on_session_init")
    """
    The session has been created. This is the first event that will
    ever be issued for a session.
    """

    SESSION_LOCAL_OPEN = _core(PN_SESSION_LOCAL_OPEN, "on_session_local_open")
    """
    The local session endpoint has been opened. Events of this type
    point to the relevant session.
    """

    SESSION_LOCAL_CLOSE = _core(PN_SESSION_LOCAL_CLOSE, "on_session_local_close")
    """
    The local session endpoint has been closed. Events of this type
    point ot the relevant session.
    """

    SESSION_REMOTE_OPEN = _core(PN_SESSION_REMOTE_OPEN, "on_session_remote_open")
    """
    The remote endpoint has opened the session. Events of this type
    point to the relevant session.
    """

    SESSION_REMOTE_CLOSE = _core(PN_SESSION_REMOTE_CLOSE, "on_session_remote_close")
    """
    The remote endpoint has closed the session. Events of this type
    point to the relevant session.
    """

    SESSION_FINAL = _core(PN_SESSION_FINAL, "on_session_final")
    """
    The session has been freed and any outstanding processing has
    been completed. This is the final event that will ever be issued
    for a session.
    """

    LINK_INIT = _core(PN_LINK_INIT, "on_link_init")
    """
    The link has been created. This is the first event that will ever
    be issued for a link.
    """

    LINK_LOCAL_OPEN = _core(PN_LINK_LOCAL_OPEN, "on_link_local_open")
    """
    The local link endpoint has been opened. Events of this type
    point ot the relevant link.
    """

    LINK_LOCAL_CLOSE = _core(PN_LINK_LOCAL_CLOSE, "on_link_local_close")
    """
    The local link endpoint has been closed. Events of this type
    point to the relevant link.
    """

    LINK_LOCAL_DETACH = _core(PN_LINK_LOCAL_DETACH, "on_link_local_detach")
    """
    The local link endpoint has been detached. Events of this type
    point to the relevant link.
    """

    LINK_REMOTE_OPEN = _core(PN_LINK_REMOTE_OPEN, "on_link_remote_open")
    """
    The remote endpoint has opened the link. Events of this type
    point to the relevant link.
    """

    LINK_REMOTE_CLOSE = _core(PN_LINK_REMOTE_CLOSE, "on_link_remote_close")
    """
    The remote endpoint has closed the link. Events of this type
    point to the relevant link.
    """

    LINK_REMOTE_DETACH = _core(PN_LINK_REMOTE_DETACH, "on_link_remote_detach")
    """
    The remote endpoint has detached the link. Events of this type
    point to the relevant link.
    """

    LINK_FLOW = _core(PN_LINK_FLOW, "on_link_flow")
    """
    The flow control state for a link has changed. Events of this
    type point to the relevant link.
    """

    LINK_FINAL = _core(PN_LINK_FINAL, "on_link_final")
    """
    The link has been freed and any outstanding processing has been
    completed. This is the final event that will ever be issued for a
    link. Events of this type point to the relevant link.
    """

    DELIVERY = _core(PN_DELIVERY, "on_delivery")
    """
    A delivery has been created or updated. Events of this type point
    to the relevant delivery.
    """

    TRANSPORT = _core(PN_TRANSPORT, "on_transport")
    """
    The transport has new data to read and/or write. Events of this
    type point to the relevant transport.
    """

    TRANSPORT_ERROR = _core(PN_TRANSPORT_ERROR, "on_transport_error")
    """
    Indicates that a transport error has occurred. Use :attr:`Transport.condition`
    to access the details of the error from the associated transport.
    """

    TRANSPORT_HEAD_CLOSED = _core(PN_TRANSPORT_HEAD_CLOSED, "on_transport_head_closed")
    """
    Indicates that the "head" or writing end of the transport has been closed. This
    means the transport will never produce more bytes for output to
    the network. Events of this type point to the relevant transport.
    """

    TRANSPORT_TAIL_CLOSED = _core(PN_TRANSPORT_TAIL_CLOSED, "on_transport_tail_closed")
    """
    Indicates that the "tail" of the transport has been closed. This
    means the transport will never be able to process more bytes from
    the network. Events of this type point to the relevant transport.
    """

    TRANSPORT_CLOSED = _core(PN_TRANSPORT_CLOSED, "on_transport_closed")
    """
    Indicates that the both the "head" and "tail" of the transport are
    closed. Events of this type point to the relevant transport.
    """

    # These events are now internal events in the python code
    REACTOR_INIT = _internal("reactor_init")
    """
    A reactor has been started. Events of this type point to the
    reactor.
    """

    REACTOR_QUIESCED = _internal("reactor_quiesced")
    """
    A reactor has no more events to process. Events of this type
    point to the reactor.
    """

    REACTOR_FINAL = _internal("reactor_final")
    """
    A reactor has been stopped. Events of this type point to the
    reactor.
    """

    SELECTABLE_INIT = _internal("selectable_init")
    SELECTABLE_UPDATED = _internal("selectable_updated")
    SELECTABLE_READABLE = _internal("selectable_readable")
    SELECTABLE_WRITABLE = _internal("selectable_writable")
    SELECTABLE_EXPIRED = _internal("selectable_expired")
    SELECTABLE_ERROR = _internal("selectable_error")
    SELECTABLE_FINAL = _internal("selectable_final")

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None

        number = pn_event_type(impl)
        cls = pn_event_class(impl)

        if cls:
            clsname = pn_class_name(cls)
            context = wrappers[clsname](pn_event_context(impl))

            # check for an application defined ApplicationEvent and return that.  This
            # avoids an expensive wrap operation invoked by event.context
            if cls == PN_PYREF and isinstance(context, EventBase):
                return context
        else:
            clsname = None

        event = Event(impl, number, clsname, context)
        return event

    def __init__(self, impl, number, clsname, context):
        self._type = EventType.TYPES[number]
        self._clsname = clsname
        self._context = context

        # Do all this messing around to avoid duplicate wrappers
        if issubclass(type(context), Delivery):
            self._delivery = context
        else:
            self._delivery = Delivery.wrap(pn_event_delivery(impl))
        if self._delivery:
            self._link = self._delivery.link
        elif issubclass(type(context), Link):
            self._link = context
        else:
            self._link = Link.wrap(pn_event_link(impl))
        if self._link:
            self._session = self._link.session
        elif issubclass(type(context), Session):
            self._session = context
        else:
            self._session = Session.wrap(pn_event_session(impl))
        if self._session:
            self._connection = self._session.connection
        elif issubclass(type(context), Connection):
            self._connection = context
        else:
            self._connection = Connection.wrap(pn_event_connection(impl))

        if issubclass(type(context), Transport):
            self._transport = context
        else:
            self._transport = Transport.wrap(pn_event_transport(impl))

    @property
    def clazz(self):
        """
        The name of the class associated with the event context.

        :type: ``str``
        """
        return self._clsname

    @property
    def context(self):
        """
        The context object associated with the event.

        :type: Depends on the type of event, and include the following:
               - :class:`Connection`
               - :class:`Session`
               - :class:`Link`
               - :class:`Delivery`
               - :class:`Transport`
        """
        return self._context

    @property
    def handler(self):
        """
        The handler for this event. The handler is determined by looking
        at the following in order:

        - The link
        - The session
        - The connection
        - The context object with an attribute "handler"

        If none of these has a handler, then ``None`` is returned.
        """
        l = self.link
        if l:
            h = l.handler
            if h:
                return h
        s = self.session
        if s:
            h = s.handler
            if h:
                return h
        c = self.connection
        if c:
            h = c.handler
            if h:
                return h
        c = self.context
        if not c or not hasattr(c, 'handler'):
            return None
        h = c.handler
        return h

    @property
    def reactor(self):
        """
        **Deprecated** - The :class:`reactor.Container` (was reactor) associated with the event.
        """
        return self.container

    @property
    def container(self):
        """
        The :class:`reactor.Container` associated with the event.
        """
        return self._transport._reactor

    def __getattr__(self, name):
        """
        This will look for a property of the event as an attached context object of the same
        type as the property (but lowercase)
        """
        c = self.context
        # Direct type or subclass of type
        if type(c).__name__.lower() == name or name in [x.__name__.lower() for x in type(c).__bases__]:
            return c

        # If the attached object is the wrong type then see if *it* has a property of that name
        return getattr(c, name, None)

    @property
    def transport(self):
        """
        The transport associated with the event, or ``None`` if none
        is associated with it.

        :type: :class:`Transport`
        """
        return self._transport

    @property
    def connection(self):
        """
        The connection associated with the event, or ``None`` if none
        is associated with it.

        :type: :class:`Connection`
        """
        return self._connection

    @property
    def session(self):
        """
        The session associated with the event, or ``None`` if none
        is associated with it.

        :type: :class:`Session`
        """
        return self._session

    @property
    def link(self):
        """
        The link associated with the event, or ``None`` if none
        is associated with it.

        :type: :class:`Link`
        """
        return self._link

    @property
    def sender(self):
        """
        The sender link associated with the event, or ``None`` if
        none is associated with it. This is essentially an alias for
        link(), that does an additional check on the type of the
        link.

        :type: :class:`Sender` (**<-- CHECK!**)
        """
        l = self.link
        if l and l.is_sender:
            return l
        else:
            return None

    @property
    def receiver(self):
        """
        The receiver link associated with the event, or ``None`` if
        none is associated with it. This is essentially an alias for
        link(), that does an additional check on the type of the link.

        :type: :class:`Receiver` (**<-- CHECK!**)
        """
        l = self.link
        if l and l.is_receiver:
            return l
        else:
            return None

    @property
    def delivery(self):
        """
        The delivery associated with the event, or ``None`` if none
        is associated with it.

        :type: :class:`Delivery`
        """
        return self._delivery


class LazyHandlers(object):
    def __get__(self, obj, clazz):
        if obj is None:
            return self
        ret = []
        obj.__dict__['handlers'] = ret
        return ret


class Handler(object):
    """
    An abstract handler for events which supports child handlers.
    """
    handlers = LazyHandlers()

    # TODO What to do with on_error?
    def add(self, handler, on_error=None):
        """
        Add a child handler

        :param handler: A child handler
        :type handler: :class:`Handler` or one of its derivatives.
        :param on_error: Not used
        """
        self.handlers.append(handler)

    def on_unhandled(self, method, *args):
        """
        The callback for handling events which are not handled by
        any other handler.

        :param method: The name of the intended handler method.
        :type method: ``str``
        :param args: Arguments for the intended handler method.
        """
        pass
