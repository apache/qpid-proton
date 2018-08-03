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

from cproton import pn_incref, pn_decref, \
    pn_py2void, pn_void2py, \
    pn_record_get, pn_record_def, pn_record_set, \
    PN_PYREF

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

    def __init__(self, impl_or_constructor, get_context=None):
        init = False
        if callable(impl_or_constructor):
            # we are constructing a new object
            impl = impl_or_constructor()
            if impl is None:
                self.__dict__["_impl"] = impl
                self.__dict__["_attrs"] = EMPTY_ATTRS
                self.__dict__["_record"] = None
                raise ProtonException(
                    "Wrapper failed to create wrapped object. Check for file descriptor or memory exhaustion.")
            init = True
        else:
            # we are wrapping an existing object
            impl = impl_or_constructor
            pn_incref(impl)

        if get_context:
            record = get_context(impl)
            attrs = pn_void2py(pn_record_get(record, PYCTX))
            if attrs is None:
                attrs = {}
                pn_record_def(record, PYCTX, PN_PYREF)
                pn_record_set(record, PYCTX, pn_py2void(attrs))
                init = True
        else:
            attrs = EMPTY_ATTRS
            init = False
            record = None
        self.__dict__["_impl"] = impl
        self.__dict__["_attrs"] = attrs
        self.__dict__["_record"] = record
        if init: self._init()

    def __getattr__(self, name):
        attrs = self.__dict__["_attrs"]
        if name in attrs:
            return attrs[name]
        else:
            raise AttributeError(name + " not in _attrs")

    def __setattr__(self, name, value):
        if hasattr(self.__class__, name):
            object.__setattr__(self, name, value)
        else:
            attrs = self.__dict__["_attrs"]
            attrs[name] = value

    def __delattr__(self, name):
        attrs = self.__dict__["_attrs"]
        if attrs:
            del attrs[name]

    def __hash__(self):
        return hash(addressof(self._impl))

    def __eq__(self, other):
        if isinstance(other, Wrapper):
            return addressof(self._impl) == addressof(other._impl)
        return False

    def __ne__(self, other):
        if isinstance(other, Wrapper):
            return addressof(self._impl) != addressof(other._impl)
        return True

    def __del__(self):
        pn_decref(self._impl)

    def __repr__(self):
        return '<%s.%s 0x%x ~ 0x%x>' % (self.__class__.__module__,
                                        self.__class__.__name__,
                                        id(self), addressof(self._impl))


PYCTX = int(pn_py2void(Wrapper))
addressof = int
