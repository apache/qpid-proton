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

import weakref

from cproton import PN_INVALID_SOCKET, \
    pn_incref, pn_decref, \
    pn_handler_add, pn_handler_clear, pn_pyhandler, \
    pn_selectable_is_reading, pn_selectable_attachments, pn_selectable_set_reading, \
    pn_selectable_expired, pn_selectable_set_fd, pn_selectable_set_registered, pn_selectable_writable, \
    pn_selectable_is_writing, pn_selectable_set_deadline, pn_selectable_is_registered, pn_selectable_terminate, \
    pn_selectable_get_deadline, pn_selectable_is_terminal, pn_selectable_readable, \
    pn_selectable_release, pn_selectable_set_writing, pn_selectable_get_fd

from ._common import millis2secs, secs2millis
from ._wrapper import Wrapper

from . import _compat

_DEFAULT = False


class Selectable(Wrapper):

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None
        else:
            return Selectable(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_selectable_attachments)

    def _init(self):
        pass

    def fileno(self, fd=_DEFAULT):
        if fd is _DEFAULT:
            return pn_selectable_get_fd(self._impl)
        elif fd is None:
            pn_selectable_set_fd(self._impl, PN_INVALID_SOCKET)
        else:
            pn_selectable_set_fd(self._impl, fd)

    def _is_reading(self):
        return pn_selectable_is_reading(self._impl)

    def _set_reading(self, val):
        pn_selectable_set_reading(self._impl, bool(val))

    reading = property(_is_reading, _set_reading)

    def _is_writing(self):
        return pn_selectable_is_writing(self._impl)

    def _set_writing(self, val):
        pn_selectable_set_writing(self._impl, bool(val))

    writing = property(_is_writing, _set_writing)

    def _get_deadline(self):
        tstamp = pn_selectable_get_deadline(self._impl)
        if tstamp:
            return millis2secs(tstamp)
        else:
            return None

    def _set_deadline(self, deadline):
        pn_selectable_set_deadline(self._impl, secs2millis(deadline))

    deadline = property(_get_deadline, _set_deadline)

    def readable(self):
        pn_selectable_readable(self._impl)

    def writable(self):
        pn_selectable_writable(self._impl)

    def expired(self):
        pn_selectable_expired(self._impl)

    def _is_registered(self):
        return pn_selectable_is_registered(self._impl)

    def _set_registered(self, registered):
        pn_selectable_set_registered(self._impl, registered)

    registered = property(_is_registered, _set_registered,
                          doc="""
The registered property may be get/set by an I/O polling system to
indicate whether the fd has been registered or not.
""")

    @property
    def is_terminal(self):
        return pn_selectable_is_terminal(self._impl)

    def terminate(self):
        pn_selectable_terminate(self._impl)

    def release(self):
        pn_selectable_release(self._impl)


class _cadapter:

    def __init__(self, handler, on_error=None):
        self.handler = handler
        self.on_error = on_error

    def dispatch(self, cevent, ctype):
        from ._events import Event
        ev = Event.wrap(cevent, ctype)
        ev.dispatch(self.handler)

    def exception(self, exc, val, tb):
        if self.on_error is None:
            _compat.raise_(exc, val, tb)
        else:
            self.on_error((exc, val, tb))


class WrappedHandlersChildSurrogate:
    def __init__(self, delegate):
        self.handlers = []
        self.delegate = weakref.ref(delegate)

    def on_unhandled(self, method, event):
        from ._events import _dispatch
        delegate = self.delegate()
        if delegate:
            _dispatch(delegate, method, event)


class WrappedHandlersProperty(object):
    def __get__(self, obj, clazz):
        if obj is None:
            return None
        return self.surrogate(obj).handlers

    def __set__(self, obj, value):
        self.surrogate(obj).handlers = value

    def surrogate(self, obj):
        key = "_surrogate"
        objdict = obj.__dict__
        surrogate = objdict.get(key, None)
        if surrogate is None:
            objdict[key] = surrogate = WrappedHandlersChildSurrogate(obj)
            obj.add(surrogate)
        return surrogate


class WrappedHandler(Wrapper):
    handlers = WrappedHandlersProperty()

    @classmethod
    def wrap(cls, impl, on_error=None):
        if impl is None:
            return None
        else:
            handler = cls(impl)
            handler.__dict__["on_error"] = on_error
            return handler

    def __init__(self, impl_or_constructor):
        Wrapper.__init__(self, impl_or_constructor)
        if list(self.__class__.__mro__).index(WrappedHandler) > 1:
            # instantiate the surrogate
            self.handlers.extend([])

    def _on_error(self, info):
        on_error = getattr(self, "on_error", None)
        if on_error is None:
            _compat.raise_(info[0], info[1], info[2])
        else:
            on_error(info)

    def add(self, handler, on_error=None):
        if handler is None: return
        if on_error is None: on_error = self._on_error
        impl = _chandler(handler, on_error)
        pn_handler_add(self._impl, impl)
        pn_decref(impl)

    def clear(self):
        pn_handler_clear(self._impl)


def _chandler(obj, on_error=None):
    if obj is None:
        return None
    elif isinstance(obj, WrappedHandler):
        impl = obj._impl
        pn_incref(impl)
        return impl
    else:
        return pn_pyhandler(_cadapter(obj, on_error))
