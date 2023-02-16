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

from typing import Any, Callable, Optional

from cproton import addressof, pn_incref, pn_decref, \
    pn_record_get_py, pn_record_def_py, pn_record_set_py

from ._exceptions import ProtonException


class EmptyAttrs:

    def __contains__(self, name):
        return False

    def __getitem__(self, name):
        raise KeyError(name)

    def __setitem__(self, name, value):
        raise TypeError("does not support item assignment")


EMPTY_ATTRS = EmptyAttrs()


class Wrapper(object):
    """ Wrapper for python objects that need to be stored in event contexts and be retrieved again from them
        Quick note on how this works:
        The actual *python* object has only 3 attributes which redirect into the wrapped C objects:
        _impl   The wrapped C object itself
        _attrs  This is a special pn_record_t holding a PYCTX which is a python dict
                every attribute in the python object is actually looked up here

        Because the objects actual attributes are stored away they must be initialised *after* the wrapping
        is set up. This is the purpose of the _init method in the wrapped  object. Wrapper.__init__ will call
        eht subclass _init to initialise attributes. So they *must not* be initialised in the subclass __init__
        before calling the superclass (Wrapper) __init__ or they will not be accessible from the wrapper at all.

    """

    def __init__(
            self,
            impl: Any = None,
            get_context: Optional[Callable[[Any], Any]] = None,
            constructor: Optional[Callable[[], Any]] = None
    ) -> None:
        init = False
        if impl is None and constructor is not None:
            # we are constructing a new object
            impl = constructor()
            if impl is None:
                self.__dict__["_impl"] = impl
                self.__dict__["_attrs"] = EMPTY_ATTRS
                raise ProtonException(
                    "Wrapper failed to create wrapped object. Check for file descriptor or memory exhaustion.")
            init = True
        else:
            # we are wrapping an existing object
            pn_incref(impl)

        if get_context:
            record = get_context(impl)
            attrs = pn_record_get_py(record)
            if attrs is None:
                attrs = {}
                pn_record_def_py(record)
                pn_record_set_py(record, attrs)
                init = True
        else:
            attrs = EMPTY_ATTRS
            init = False
        self.__dict__["_impl"] = impl
        self.__dict__["_attrs"] = attrs
        if init:
            self._init()

    def __getattr__(self, name: str) -> Any:
        attrs = self.__dict__["_attrs"]
        if name in attrs:
            return attrs[name]
        else:
            raise AttributeError(name + " not in _attrs")

    def __setattr__(self, name: str, value: Any) -> None:
        if hasattr(self.__class__, name):
            object.__setattr__(self, name, value)
        else:
            attrs = self.__dict__["_attrs"]
            attrs[name] = value

    def __delattr__(self, name: str) -> None:
        attrs = self.__dict__["_attrs"]
        if attrs:
            del attrs[name]

    def __hash__(self) -> int:
        return hash(addressof(self._impl))

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, Wrapper):
            return addressof(self._impl) == addressof(other._impl)
        return False

    def __ne__(self, other: Any) -> bool:
        if isinstance(other, Wrapper):
            return addressof(self._impl) != addressof(other._impl)
        return True

    def __del__(self) -> None:
        pn_decref(self._impl)

    def __repr__(self) -> str:
        return '<%s.%s 0x%x ~ 0x%x>' % (self.__class__.__module__,
                                        self.__class__.__name__,
                                        id(self), addressof(self._impl))
