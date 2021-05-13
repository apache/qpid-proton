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

import uuid

from cproton import PN_ARRAY, PN_BINARY, PN_BOOL, PN_BYTE, PN_CHAR, PN_DECIMAL128, PN_DECIMAL32, PN_DECIMAL64, \
    PN_DESCRIBED, PN_DOUBLE, PN_FLOAT, PN_INT, PN_LIST, PN_LONG, PN_MAP, PN_NULL, PN_OVERFLOW, PN_SHORT, PN_STRING, \
    PN_SYMBOL, PN_TIMESTAMP, PN_UBYTE, PN_UINT, PN_ULONG, PN_USHORT, PN_UUID, pn_data, pn_data_clear, pn_data_copy, \
    pn_data_decode, pn_data_dump, pn_data_encode, pn_data_encoded_size, pn_data_enter, pn_data_error, pn_data_exit, \
    pn_data_format, pn_data_free, pn_data_get_array, pn_data_get_array_type, pn_data_get_binary, pn_data_get_bool, \
    pn_data_get_byte, pn_data_get_char, pn_data_get_decimal128, pn_data_get_decimal32, pn_data_get_decimal64, \
    pn_data_get_double, pn_data_get_float, pn_data_get_int, pn_data_get_list, pn_data_get_long, pn_data_get_map, \
    pn_data_get_short, pn_data_get_string, pn_data_get_symbol, pn_data_get_timestamp, pn_data_get_ubyte, \
    pn_data_get_uint, pn_data_get_ulong, pn_data_get_ushort, pn_data_get_uuid, pn_data_is_array_described, \
    pn_data_is_described, pn_data_is_null, pn_data_lookup, pn_data_narrow, pn_data_next, pn_data_prev, \
    pn_data_put_array, pn_data_put_binary, pn_data_put_bool, pn_data_put_byte, pn_data_put_char, pn_data_put_decimal128, \
    pn_data_put_decimal32, pn_data_put_decimal64, pn_data_put_described, pn_data_put_double, pn_data_put_float, \
    pn_data_put_int, pn_data_put_list, pn_data_put_long, pn_data_put_map, pn_data_put_null, pn_data_put_short, \
    pn_data_put_string, pn_data_put_symbol, pn_data_put_timestamp, pn_data_put_ubyte, pn_data_put_uint, \
    pn_data_put_ulong, pn_data_put_ushort, pn_data_put_uuid, pn_data_rewind, pn_data_type, pn_data_widen, pn_error_text

from ._common import Constant
from ._exceptions import DataException, EXCEPTIONS

long = int
unicode = str


class UnmappedType:

    def __init__(self, msg):
        self.msg = msg

    def __repr__(self):
        return "UnmappedType(%s)" % self.msg


class ulong(long):
    """
    The ulong AMQP type.

    An unsigned 64 bit integer in the range :math:`0` to :math:`2^{64} - 1` inclusive.
    """

    def __init__(self, l):
        if (l < 0):
            raise AssertionError("initializing ulong with negative value")
        super(ulong, self).__new__(ulong, l)

    def __repr__(self):
        return "ulong(%s)" % long.__repr__(self)


class timestamp(long):
    """
    The timestamp AMQP type.

    An absolute point in time, represented by a signed 64 bit value measuring
    milliseconds since the epoch. This value is encoded using the Unix ``time_t``
    [IEEE1003] encoding of UTC, but with a precision of milliseconds. For
    example, ``1311704463521`` represents the moment ``2011-07-26T18:21:03.521Z``.
    """

    def __repr__(self):
        return "timestamp(%s)" % long.__repr__(self)


class symbol(unicode):
    """
    The symbol AMQP type.

    Symbolic values from a constrained domain, represented by a sequence of ASCII characters.
    """

    def __repr__(self):
        return "symbol(%s)" % unicode.__repr__(self)


class char(unicode):
    """
    The char AMQP type.

    A 32 bit UTF-32BE encoded Unicode character.
    """

    def __repr__(self):
        return "char(%s)" % unicode.__repr__(self)


class byte(int):
    """
    The byte AMQP type.

    An 8 bit signed integer in the range :math:`-(2^7)` to :math:`2^7 - 1` inclusive.
    """

    def __repr__(self):
        return "byte(%s)" % int.__repr__(self)


class short(int):
    """
    The short AMQP type.

    A 16 bit signed integer in the range :math:`-(2^{15})` to :math:`2^{15} - 1` inclusive.
    """

    def __repr__(self):
        return "short(%s)" % int.__repr__(self)


class int32(int):
    """
    The signed int AMQP type.

    A 32 bit signed integer in the range :math:`-(2^{31})` to :math:`2^{31} - 1` inclusive.
    """

    def __repr__(self):
        return "int32(%s)" % int.__repr__(self)


class ubyte(int):
    """
    The unsigned byte AMQP type.

    An 8 bit unsigned integer in the range :math:`0` to :math:`2^8 - 1` inclusive.
    """

    def __init__(self, i):
        if (i < 0):
            raise AssertionError("initializing ubyte with negative value")
        super(ubyte, self).__new__(ubyte, i)

    def __repr__(self):
        return "ubyte(%s)" % int.__repr__(self)


class ushort(int):
    """
    The unsigned short AMQP type.

    A 16 bit unsigned integer in the range :math:`0` to :math:`2^{16} - 1` inclusive.
    """

    def __init__(self, i):
        if (i < 0):
            raise AssertionError("initializing ushort with negative value")
        super(ushort, self).__new__(ushort, i)

    def __repr__(self):
        return "ushort(%s)" % int.__repr__(self)


class uint(long):
    """
    The unsigned int AMQP type.

    A 32 bit unsigned integer in the range :math:`0` to :math:`2^{32} - 1` inclusive.
    """

    def __init__(self, l):
        if (l < 0):
            raise AssertionError("initializing uint with negative value")
        super(uint, self).__new__(uint, l)

    def __repr__(self):
        return "uint(%s)" % long.__repr__(self)


class float32(float):
    """
    The float AMQP type.

    A 32 bit floating point number (IEEE 754-2008 binary32).
    """

    def __repr__(self):
        return "float32(%s)" % float.__repr__(self)


class decimal32(int):
    """
    The decimal32 AMQP type.

    A 32 bit decimal floating point  number (IEEE 754-2008 decimal32).
    """

    def __repr__(self):
        return "decimal32(%s)" % int.__repr__(self)


class decimal64(long):
    """
    The decimal64 AMQP type.

    A 64 bit decimal floating point number (IEEE 754-2008 decimal64).
    """

    def __repr__(self):
        return "decimal64(%s)" % long.__repr__(self)


class decimal128(bytes):
    """
    The decimal128 AMQP type.

    A 128-bit decimal floating-point number (IEEE 754-2008 decimal128).
    """

    def __repr__(self):
        return "decimal128(%s)" % bytes.__repr__(self)


class Described(object):
    """
    A described AMQP type.

    :ivar descriptor: A symbol describing the value.
    :vartype descriptor: :class:`symbol`
    :ivar value: The described value
    :vartype value: Any AMQP value
    """

    def __init__(self, descriptor, value):
        self.descriptor = descriptor
        self.value = value

    def __repr__(self):
        return "Described(%r, %r)" % (self.descriptor, self.value)

    def __eq__(self, o):
        if isinstance(o, Described):
            return self.descriptor == o.descriptor and self.value == o.value
        else:
            return False


UNDESCRIBED = Constant("UNDESCRIBED")


class Array(object):
    """
    An AMQP array, a sequence of AMQP values of a single type.

    This class provides a convenient way to handle AMQP arrays when used with
    convenience methods :func:`Data.get_py_array` and :func:`Data.put_py_array`.

    :ivar descriptor: Optional descriptor if the array is to be described, otherwise ``None``
    :ivar type: Array element type, as an integer. The :class:`Data` class has constants defined
        for all the valid AMQP types. For example, for an array of double values, use
        :const:`Data.DOUBLE`, which has integer value 14.
    :ivar elements: A Python list of elements of the appropriate type.
    """

    def __init__(self, descriptor, type, *elements):
        self.descriptor = descriptor
        self.type = type
        self.elements = elements

    def __iter__(self):
        return iter(self.elements)

    def __repr__(self):
        if self.elements:
            els = ", %s" % (", ".join(map(repr, self.elements)))
        else:
            els = ""
        return "Array(%r, %r%s)" % (self.descriptor, self.type, els)

    def __eq__(self, o):
        if isinstance(o, Array):
            return self.descriptor == o.descriptor and \
                self.type == o.type and self.elements == o.elements
        else:
            return False


def _check_type(s, allow_ulong=False, raise_on_error=True):
    if isinstance(s, symbol):
        return s
    if allow_ulong and isinstance(s, ulong):
        return s
    if isinstance(s, str):
        # Must be py2 or py3 str
        return symbol(s)
    if isinstance(s, unicode):
        # This must be python2 unicode as we already detected py3 str above
        return symbol(s.encode('utf-8'))
    if raise_on_error:
        raise TypeError('Non-symbol type %s: %s' % (type(s), s))
    return s


def _check_is_symbol(s, raise_on_error=True):
    return _check_type(s, allow_ulong=False, raise_on_error=raise_on_error)


def _check_is_symbol_or_ulong(s, raise_on_error=True):
    return _check_type(s, allow_ulong=True, raise_on_error=raise_on_error)


class RestrictedKeyDict(dict):
    """Parent class for :class:`PropertyDict` and :class:`AnnotationDict`"""

    def __init__(self, validation_fn, e=None, raise_on_error=True, **kwargs):
        super(RestrictedKeyDict, self).__init__()
        self.validation_fn = validation_fn
        self.raise_on_error = raise_on_error
        self.update(e, **kwargs)

    def __setitem__(self, key, value):
        """Checks if the key is a :class:`symbol` type before setting the value"""
        try:
            return super(RestrictedKeyDict, self).__setitem__(self.validation_fn(key, self.raise_on_error), value)
        except TypeError:
            pass
        # __setitem__() must raise a KeyError, not TypeError
        raise KeyError('invalid non-symbol key: %s: %s' % (type(key), key))

    def update(self, e=None, **kwargs):
        """
        Equivalent to dict.update(), but it was needed to call :meth:`__setitem__()`
        instead of ``dict.__setitem__()``.
        """
        if e:
            try:
                for k in e:
                    self.__setitem__(k, e[k])
            except TypeError:
                self.__setitem__(k[0], k[1])  # use tuple consumed from from zip
                for (k, v) in e:
                    self.__setitem__(k, v)
        for k in kwargs:
            self.__setitem__(k, kwargs[k])


class PropertyDict(RestrictedKeyDict):
    """
    A dictionary that only takes :class:`symbol` types as a key.
    However, if a string key is provided, it will be silently converted
    into a symbol key.

        >>> from proton import symbol, ulong, PropertyDict
        >>> a = PropertyDict(one=1, two=2)
        >>> b = PropertyDict({'one':1, symbol('two'):2})
        >>> c = PropertyDict(zip(['one', symbol('two')], [1, 2]))
        >>> d = PropertyDict([(symbol('one'), 1), ('two', 2)])
        >>> e = PropertyDict(a)
        >>> a == b == c == d == e
        True

    By default, non-string and non-symbol keys cause a ``KeyError`` to be raised:

        >>> PropertyDict({'one':1, 2:'two'})
          ...
        KeyError: "invalid non-symbol key: <type 'int'>: 2"

    but by setting ``raise_on_error=False``, non-string and non-symbol keys will be ignored:

        >>> PropertyDict({'one':1, 2:'two'}, raise_on_error=False)
        PropertyDict({2: 'two', symbol(u'one'): 1})

    :param e: Initialization for ``dict``
    :type e: ``dict`` or ``list`` of ``tuple`` or ``zip`` object
    :param raise_on_error: If ``True``, will raise an ``KeyError`` if a non-string or non-symbol
        is encountered as a key in the initialization, or in a subsequent operation which
        adds such an key. If ``False``, non-strings and non-symbols will be added as keys
        to the dictionary without an error.
    :type raise_on_error: ``bool``
    :param kwargs: Keyword args for initializing a ``dict`` of the form key1=val1, key2=val2, ...
    """

    def __init__(self, e=None, raise_on_error=True, **kwargs):
        super(PropertyDict, self).__init__(_check_is_symbol, e, raise_on_error, **kwargs)

    def __repr__(self):
        """ Representation of PropertyDict """
        return 'PropertyDict(%s)' % super(PropertyDict, self).__repr__()


class AnnotationDict(RestrictedKeyDict):
    """
    A dictionary that only takes :class:`symbol` or :class:`ulong` types
    as a key. However, if a string key is provided, it will be silently
    converted into a symbol key.

        >>> from proton import symbol, ulong, AnnotationDict
        >>> a = AnnotationDict(one=1, two=2)
        >>> a[ulong(3)] = 'three'
        >>> b = AnnotationDict({'one':1, symbol('two'):2, ulong(3):'three'})
        >>> c = AnnotationDict(zip([symbol('one'), 'two', ulong(3)], [1, 2, 'three']))
        >>> d = AnnotationDict([('one', 1), (symbol('two'), 2), (ulong(3), 'three')])
        >>> e = AnnotationDict(a)
        >>> a == b == c == d == e
        True

    By default, non-string, non-symbol and non-ulong keys cause a ``KeyError`` to be raised:

        >>> AnnotationDict({'one': 1, 2: 'two'})
          ...
        KeyError: "invalid non-symbol key: <type 'int'>: 2"

    but by setting ``raise_on_error=False``, non-string, non-symbol and non-ulong keys will be ignored:

        >>> AnnotationDict({'one': 1, 2: 'two'}, raise_on_error=False)
        AnnotationDict({2: 'two', symbol(u'one'): 1})

    :param e: Initialization for ``dict``
    :type e: ``dict`` or ``list`` of ``tuple`` or ``zip`` object
    :param raise_on_error: If ``True``, will raise an ``KeyError`` if a non-string, non-symbol or
        non-ulong is encountered as a key in the initialization, or in a subsequent
        operation which adds such an key. If ``False``, non-strings, non-ulongs and non-symbols
        will be added as keys to the dictionary without an error.
    :type raise_on_error: ``bool``
    :param kwargs: Keyword args for initializing a ``dict`` of the form key1=val1, key2=val2, ...
    """

    def __init__(self, e=None, raise_on_error=True, **kwargs):
        super(AnnotationDict, self).__init__(_check_is_symbol_or_ulong, e, raise_on_error, **kwargs)

    def __repr__(self):
        """ Representation of AnnotationDict """
        return 'AnnotationDict(%s)' % super(AnnotationDict, self).__repr__()


class SymbolList(list):
    """
    A list that can only hold :class:`symbol` elements. However, if any string elements
    are present, they will be converted to symbols.

        >>> a = SymbolList(['one', symbol('two'), 'three'])
        >>> b = SymbolList([symbol('one'), 'two', symbol('three')])
        >>> c = SymbolList(a)
        >>> a == b == c
        True

    By default, using any key other than a symbol or string will result in a ``TypeError``:

        >>> SymbolList(['one', symbol('two'), 3])
          ...
        TypeError: Non-symbol type <class 'int'>: 3

    but by setting ``raise_on_error=False``, non-symbol and non-string keys will be ignored:

        >>> SymbolList(['one', symbol('two'), 3], raise_on_error=False)
        SymbolList([symbol(u'one'), symbol(u'two'), 3])

    :param t: Initialization for list
    :type t: ``list``
    :param raise_on_error: If ``True``, will raise an ``TypeError`` if a non-string or non-symbol is
        encountered in the initialization list, or in a subsequent operation which adds such
        an element. If ``False``, non-strings and non-symbols will be added to the list without
        an error.
    :type raise_on_error: ``bool``
    """

    def __init__(self, t=None, raise_on_error=True):
        super(SymbolList, self).__init__()
        self.raise_on_error = raise_on_error
        if t:
            self.extend(t)

    def _check_list(self, t):
        """ Check all items in list are :class:`symbol`s (or are converted to symbols). """
        l = []
        if t:
            for v in t:
                l.append(_check_is_symbol(v, self.raise_on_error))
        return l

    def append(self, v):
        """ Add a single value v to the end of the list """
        return super(SymbolList, self).append(_check_is_symbol(v, self.raise_on_error))

    def extend(self, t):
        """ Add all elements of an iterable t to the end of the list """
        return super(SymbolList, self).extend(self._check_list(t))

    def insert(self, i, v):
        """ Insert a value v at index i """
        return super(SymbolList, self).insert(i, _check_is_symbol(v, self.raise_on_error))

    def __add__(self, t):
        """ Handles list1 + list2 """
        return SymbolList(super(SymbolList, self).__add__(self._check_list(t)), raise_on_error=self.raise_on_error)

    def __iadd__(self, t):
        """ Handles list1 += list2 """
        return super(SymbolList, self).__iadd__(self._check_list(t))

    def __setitem__(self, i, t):
        """ Handles list[i] = v """
        return super(SymbolList, self).__setitem__(i, _check_is_symbol(t, self.raise_on_error))

    def __repr__(self):
        """ Representation of SymbolList """
        return 'SymbolList(%s)' % super(SymbolList, self).__repr__()


class Data:
    """
    The :class:`Data` class provides an interface for decoding, extracting,
    creating, and encoding arbitrary AMQP data. A :class:`Data` object
    contains a tree of AMQP values. Leaf nodes in this tree correspond
    to scalars in the AMQP type system such as :const:`ints <INT>` or
    :const:`strings <STRING>`. Non-leaf nodes in this tree correspond to
    compound values in the AMQP type system such as :const:`lists <LIST>`,
    :const:`maps <MAP>`, :const:`arrays <ARRAY>`, or :const:`described values <DESCRIBED>`.
    The root node of the tree is the :class:`Data` object itself and can have
    an arbitrary number of children.

    A :class:`Data` object maintains the notion of the current sibling node
    and a current parent node. Siblings are ordered within their parent.
    Values are accessed and/or added by using the :meth:`next`, :meth:`prev`,
    :meth:`enter`, and :meth:`exit` methods to navigate to the desired location in
    the tree and using the supplied variety of ``put_*`` / ``get_*`` methods to
    access or add a value of the desired type.

    The ``put_*`` methods will always add a value *after* the current node
    in the tree. If the current node has a next sibling the ``put_*`` method
    will overwrite the value on this node. If there is no current node
    or the current node has no next sibling then one will be added. The
    ``put_*`` methods always set the added/modified node to the current
    node. The ``get_*`` methods read the value of the current node and do
    not change which node is current.

    The following types of scalar values are supported:

        * :const:`NULL`
        * :const:`BOOL`
        * :const:`UBYTE`
        * :const:`USHORT`
        * :const:`SHORT`
        * :const:`UINT`
        * :const:`INT`
        * :const:`ULONG`
        * :const:`LONG`
        * :const:`FLOAT`
        * :const:`DOUBLE`
        * :const:`BINARY`
        * :const:`STRING`
        * :const:`SYMBOL`

    The following types of compound values are supported:

        * :const:`DESCRIBED`
        * :const:`ARRAY`
        * :const:`LIST`
        * :const:`MAP`
    """

    NULL = PN_NULL  #: A null value.
    BOOL = PN_BOOL  #: A boolean value.
    UBYTE = PN_UBYTE  #: An unsigned byte value.
    BYTE = PN_BYTE  #: A signed byte value.
    USHORT = PN_USHORT  #: An unsigned short value.
    SHORT = PN_SHORT  #: A short value.
    UINT = PN_UINT  #: An unsigned int value.
    INT = PN_INT  #: A signed int value.
    CHAR = PN_CHAR  #: A character value.
    ULONG = PN_ULONG  #: An unsigned long value.
    LONG = PN_LONG  #: A signed long value.
    TIMESTAMP = PN_TIMESTAMP  #: A timestamp value.
    FLOAT = PN_FLOAT  #: A float value.
    DOUBLE = PN_DOUBLE  #: A double value.
    DECIMAL32 = PN_DECIMAL32  #: A DECIMAL32 value.
    DECIMAL64 = PN_DECIMAL64  #: A DECIMAL64 value.
    DECIMAL128 = PN_DECIMAL128  #: A DECIMAL128 value.
    UUID = PN_UUID  #: A UUID value.
    BINARY = PN_BINARY  #: A binary string.
    STRING = PN_STRING  #: A unicode string.
    SYMBOL = PN_SYMBOL  #: A symbolic string.
    DESCRIBED = PN_DESCRIBED  #: A described value.
    ARRAY = PN_ARRAY  #: An array value.
    LIST = PN_LIST  #: A list value.
    MAP = PN_MAP  #: A map value.

    type_names = {
        NULL: "null",
        BOOL: "bool",
        BYTE: "byte",
        UBYTE: "ubyte",
        SHORT: "short",
        USHORT: "ushort",
        INT: "int",
        UINT: "uint",
        CHAR: "char",
        LONG: "long",
        ULONG: "ulong",
        TIMESTAMP: "timestamp",
        FLOAT: "float",
        DOUBLE: "double",
        DECIMAL32: "decimal32",
        DECIMAL64: "decimal64",
        DECIMAL128: "decimal128",
        UUID: "uuid",
        BINARY: "binary",
        STRING: "string",
        SYMBOL: "symbol",
        DESCRIBED: "described",
        ARRAY: "array",
        LIST: "list",
        MAP: "map"
    }
    """
    A map which uses the enumerated type as a key to get a text name for the type.
    """

    @classmethod
    def type_name(amqptype):
        """
        Return a string name for an AMQP type.

        :param amqptype: Numeric Proton AMQP type (`enum pn_type_t`)
        :type amqptype: integer
        :rtype: String describing the AMQP type with numeric value `amqptype`
        """
        return Data.type_names[amqptype]

    def __init__(self, capacity=16):
        if isinstance(capacity, (int, long)):
            self._data = pn_data(capacity)
            self._free = True
        else:
            self._data = capacity
            self._free = False

    def __del__(self):
        if self._free and hasattr(self, "_data"):
            pn_data_free(self._data)
            del self._data

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, DataException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_data_error(self._data))))
        else:
            return err

    def clear(self):
        """
        Clears the data object.
        """
        pn_data_clear(self._data)

    def rewind(self):
        """
        Clears current node and sets the parent to the root node.  Clearing the
        current node sets it _before_ the first node, calling next() will advance to
        the first node.
        """
        assert self._data is not None
        pn_data_rewind(self._data)

    def next(self):
        """
        Advances the current node to its next sibling and returns its
        type. If there is no next sibling the current node remains
        unchanged and ``None`` is returned.

        :return: Node type or ``None``
        :rtype: ``int`` or ``None``
        """
        found = pn_data_next(self._data)
        if found:
            return self.type()
        else:
            return None

    def prev(self):
        """
        Advances the current node to its previous sibling and returns its
        type. If there is no previous sibling the current node remains
        unchanged and ``None`` is returned.

        :return: Node type or ``None``
        :rtype: ``int`` or ``None``
        """
        found = pn_data_prev(self._data)
        if found:
            return self.type()
        else:
            return None

    def enter(self):
        """
        Sets the parent node to the current node and clears the current node.
        Clearing the current node sets it *before* the first child,
        call :meth:`next` to advance to the first child.

        :return: ``True`` iff the pointers to the current/parent nodes are changed,
            ``False`` otherwise.
        :rtype: ``bool``
        """
        return pn_data_enter(self._data)

    def exit(self):
        """
        Sets the current node to the parent node and the parent node to
        its own parent.

        :return: ``True`` iff the pointers to the current/parent nodes are changed,
            ``False`` otherwise.
        :rtype: ``bool``
        """
        return pn_data_exit(self._data)

    def lookup(self, name):
        return pn_data_lookup(self._data, name)

    def narrow(self):
        """
        Modify this :class:`Data` object to behave as if the current node is the
        root node of the tree. This impacts the behavior of :meth:`rewind`,
        :meth:`next`, :meth:`prev`, and anything else that depends on the
        navigational state of the :class:`Data` object. Use :meth:`widen`
        to reverse the effect of this operation.
        """
        pn_data_narrow(self._data)

    def widen(self):
        """ Reverse the effect of :meth:`narrow`. """
        pn_data_widen(self._data)

    def type(self):
        """
        Returns the type of the current node. Returns `None` if there is no
        current node.

        :return: The current node type enumeration.
        :rtype: ``int`` or ``None``
        """
        dtype = pn_data_type(self._data)
        if dtype == -1:
            return None
        else:
            return dtype

    def encoded_size(self):
        """
        Returns the size in bytes needed to encode the data in AMQP format.

        :return: The size of the encoded data or an error code if data is invalid.
        :rtype: ``int``
        """
        return pn_data_encoded_size(self._data)

    def encode(self):
        """
        Returns a representation of the data encoded in AMQP format.

        :return: The size of the encoded data
        :rtype: ``int``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        size = 1024
        while True:
            cd, enc = pn_data_encode(self._data, size)
            if cd == PN_OVERFLOW:
                size *= 2
            elif cd >= 0:
                return enc
            else:
                self._check(cd)

    def decode(self, encoded):
        """
        Decodes the first value from supplied AMQP data and returns the
        number of bytes consumed.

        :type encoded: binary
        :param encoded: AMQP encoded binary data
        :raise: :exc:`DataException` if there is a Proton error.
        """
        return self._check(pn_data_decode(self._data, encoded))

    def put_list(self):
        """
        Puts a list value. Elements may be filled by entering the list
        node and putting element values.

            >>> data = Data()
            >>> data.put_list()
            >>> data.enter()
            >>> data.put_int(1)
            >>> data.put_int(2)
            >>> data.put_int(3)
            >>> data.exit()

        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_list(self._data))

    def put_map(self):
        """
        Puts a map value. Elements may be filled by entering the map node
        and putting alternating key value pairs.

            >>> data = Data()
            >>> data.put_map()
            >>> data.enter()
            >>> data.put_string("key")
            >>> data.put_string("value")
            >>> data.exit()

        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_map(self._data))

    def put_array(self, described, element_type):
        """
        Puts an array value. Elements may be filled by entering the array
        node and putting the element values. The values must all be of the
        specified array element type. If an array is described then the
        first child value of the array is the descriptor and may be of any
        type.

            >>> data = Data()
            >>>
            >>> data.put_array(False, Data.INT)
            >>> data.enter()
            >>> data.put_int(1)
            >>> data.put_int(2)
            >>> data.put_int(3)
            >>> data.exit()
            >>>
            >>> data.put_array(True, Data.DOUBLE)
            >>> data.enter()
            >>> data.put_symbol("array-descriptor")
            >>> data.put_double(1.1)
            >>> data.put_double(1.2)
            >>> data.put_double(1.3)
            >>> data.exit()

        :type described: bool
        :param described: specifies whether the array is described
        :type element_type: int
        :param element_type: the type of the array elements
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_array(self._data, described, element_type))

    def put_described(self):
        """
        Puts a described value. A described node has two children, the
        descriptor and the value. These are specified by entering the node
        and putting the desired values.

            >>> data = Data()
            >>> data.put_described()
            >>> data.enter()
            >>> data.put_symbol("value-descriptor")
            >>> data.put_string("the value")
            >>> data.exit()

        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_described(self._data))

    def put_null(self):
        """
        Puts a null value.

        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_null(self._data))

    def put_bool(self, b):
        """
        Puts a boolean value.

        :param b: a boolean value
        :type b: ``bool`` or ``int``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_bool(self._data, b))

    def put_ubyte(self, ub):
        """
        Puts an unsigned byte value.

        :param ub: an integral value in the range :math:`0` to :math:`2^8 - 1` inclusive
        :type ub: ``int``, :class:`ubyte`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`0` to :math:`2^8 - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_ubyte(self._data, ub))

    def put_byte(self, b):
        """
        Puts a signed byte value.

        :param b: an integral value in the range :math:`-(2^7)` to :math:`2^7 - 1` inclusive.
        :type b: ``int``, :class:`byte`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`-(2^7)` to :math:`2^7 - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_byte(self._data, b))

    def put_ushort(self, us):
        """
        Puts an unsigned short value.

        :param us: an integral value in the range :math:`0` to :math:`2^{16} - 1` inclusive.
        :type us: ``int``, :class:`ushort`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`0` to :math:`2^{16} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_ushort(self._data, us))

    def put_short(self, s):
        """
        Puts a signed short value.

        :param s: an integral value in the range :math:`-(2^{15})` to :math:`2^{15} - 1` inclusive.
        :type s: ``int``, :class:`short`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`-(2^{15})` to :math:`2^{15} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_short(self._data, s))

    def put_uint(self, ui):
        """
        Puts an unsigned int value.

        :param ui: an integral value in the range :math:`0` to :math:`2^{32} - 1` inclusive.
        :type ui: ``int``, :class:`uint`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`0` to :math:`2^{32} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_uint(self._data, ui))

    def put_int(self, i):
        """
        Puts a signed int value.

        :param i: an integral value in the range :math:`-(2^{31})` to :math:`2^{31} - 1` inclusive.
        :type i: ``int``, :class:`int32`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`-(2^{31})` to :math:`2^{31} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_int(self._data, i))

    def put_char(self, c):
        """
        Puts a char value.

        :param c: a single character
        :type c: ``str``, :class:`char`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_char(self._data, ord(c)))

    def put_ulong(self, ul):
        """
        Puts an unsigned long value.

        :param ul: an integral value in the range :math:`0` to :math:`2^{64} - 1` inclusive.
        :type ul: ``int``, ``long``, :class:`ulong`
        :raise: * ``AssertionError`` if parameter is out of the range :math:`0` to :math:`2^{64} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_ulong(self._data, ul))

    def put_long(self, l):
        """
        Puts a signed long value.

        :param l: an integral value in the range :math:`-(2^{63})` to :math:`2^{63} - 1` inclusive.
        :type l: ``int``, ``long``
        :raise: * ``AssertionError`` if parameter is out of the range :math:`-(2^{63})` to :math:`2^{63} - 1` inclusive.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_long(self._data, l))

    def put_timestamp(self, t):
        """
        Puts a timestamp value.

        :param t: a positive integral value
        :type t: ``int``, :class:`timestamp`
        :raise: * ``AssertionError`` if parameter is negative.
                * :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_timestamp(self._data, t))

    def put_float(self, f):
        """
        Puts a float value.

        :param f: a floating point value
        :type f: ``float``, :class:`float32`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_float(self._data, f))

    def put_double(self, d):
        """
        Puts a double value.

        :param d: a floating point value.
        :type d: ``double``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_double(self._data, d))

    def put_decimal32(self, d):
        """
        Puts a decimal32 value.

        :param d: a decimal32 number encoded in an 32-bit integral value.
        :type d: ``int``, :class:`decimal32`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_decimal32(self._data, d))

    def put_decimal64(self, d):
        """
        Puts a decimal64 value.

        :param d: a decimal64 number encoded in an 32-bit integral value.
        :type d: ``int``, ``long``, :class:`decimal64`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_decimal64(self._data, d))

    def put_decimal128(self, d):
        """
        Puts a decimal128 value.

        :param d: a decimal128 value encoded in a 16-byte binary value.
        :type d: ``bytes``, :class:`decimal128`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_decimal128(self._data, d))

    def put_uuid(self, u):
        """
        Puts a UUID value.

        :param u: a uuid value.
        :type u: ``uuid.UUID``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_uuid(self._data, u.bytes))

    def put_binary(self, b):
        """
        Puts a binary value.

        :param b: a binary value
        :type b: ``bytes``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_binary(self._data, b))

    def put_memoryview(self, mv):
        """
        Put a Python memoryview object as an AMQP binary value.

        :param mv: A Python memoryview object
        :type mv: ``memoryview``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self.put_binary(mv.tobytes())

    def put_buffer(self, buff):
        """
        Put a Python buffer object as an AMQP binary value.

        :param buff: A Python buffer object (**CHECK THIS**)
        :type buff: Any object supporting the Python buffer interface.
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self.put_binary(bytes(buff))

    def put_string(self, s):
        """
        Puts a unicode value.

        :param s: a unicode string
        :type s: ``str`` (Python 3.x) or ``unicode`` (Python 2.x)
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_string(self._data, s.encode("utf8")))

    def put_symbol(self, s):
        """
        Puts a symbolic value.

        :param s: the symbol name
        :type s: string, :class:`symbol`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_put_symbol(self._data, s.encode('ascii')))

    def get_list(self):
        """
        If the current node is a list, return the number of elements,
        otherwise return 0. List elements can be accessed by entering
        the list.

            >>> count = data.get_list()
            >>> data.enter()
            >>> for i in range(count):
            ...   type = data.next()
            ...   if type == Data.STRING:
            ...     print data.get_string()
            ...   elif type == ...:
            ...     ...
            >>> data.exit()

        :return: the number of child elements of a list node
        :rtype: ``int``
        """
        return pn_data_get_list(self._data)

    def get_map(self):
        """
        If the current node is a map, return the number of child elements,
        otherwise return 0. Key value pairs can be accessed by entering
        the map.

            >>> count = data.get_map()
            >>> data.enter()
            >>> for i in range(count/2):
            ...   type = data.next()
            ...   if type == Data.STRING:
            ...     print data.get_string()
            ...   elif type == ...:
            ...     ...
            >>> data.exit()

        :return: the number of child elements of a map node
        :rtype: ``int``
        """
        return pn_data_get_map(self._data)

    def get_array(self):
        """
        If the current node is an array, return a tuple of the element
        count, a boolean indicating whether the array is described, and
        the type of each element, otherwise return ``None``. Array
        data can be accessed by entering the array.

            >>> # read an array of strings with a symbolic descriptor
            >>> count, described, type = data.get_array()
            >>> data.enter()
            >>> data.next()
            >>> print "Descriptor:", data.get_symbol()
            >>> for i in range(count):
            ...    data.next()
            ...    print "Element:", data.get_string()
            >>> data.exit()

        :return: A tuple containing the number of array elements, the descriptor
            (or ``None`` if no descriptor) and the enumerated array element type.
        :rtype: ``tuple`` (``int``, ``str`` or ``None``, ``int``)
        """
        count = pn_data_get_array(self._data)
        described = pn_data_is_array_described(self._data)
        type = pn_data_get_array_type(self._data)
        if type == -1:
            type = None
        return count, described, type

    def is_described(self):
        """
        Checks if the current node is a described value. The descriptor
        and value may be accessed by entering the described value.

            >>> # read a symbolically described string
            >>> assert data.is_described() # will error if the current node is not described
            >>> data.enter()
            >>> data.next()
            >>> print data.get_symbol()
            >>> data.next()
            >>> print data.get_string()
            >>> data.exit()

        :return: ``True`` if the current node is a described type, ``False`` otherwise.
        :rtype: ``bool``
        """
        return pn_data_is_described(self._data)

    def is_null(self):
        """
        Checks if the current node is the AMQP null type.

        :return: ``True`` if the current node is the AMQP null type, ``False`` otherwise.
        :rtype: ``bool``
        """
        return pn_data_is_null(self._data)

    def get_bool(self):
        """
        Get the current node value as a ``bool``.

        :return: If the current node is a boolean type, returns its value,
            ``False`` otherwise.
        :rtype: ``bool``
        """
        return pn_data_get_bool(self._data)

    def get_ubyte(self):
        """
        Get the current node value as a :class:`ubyte`.

        :return: If the current node is an unsigned byte, its value, 0 otherwise.
        :rtype: :class:`ubyte`
        """
        return ubyte(pn_data_get_ubyte(self._data))

    def get_byte(self):
        """
        Get the current node value as a :class:`byte`.

        :return: If the current node is a signed byte, its value, 0 otherwise.
        :rtype: :class:`byte`
        """
        return byte(pn_data_get_byte(self._data))

    def get_ushort(self):
        """
        Get the current node value as a :class:`ushort`.

        :return: If the current node is an unsigned short, its value, 0 otherwise.
        :rtype: :class:`ushort`
        """
        return ushort(pn_data_get_ushort(self._data))

    def get_short(self):
        """
        Get the current node value as a :class:`short`.

        :return: If the current node is a signed short, its value, 0 otherwise.
        :rtype: :class:`short`
        """
        return short(pn_data_get_short(self._data))

    def get_uint(self):
        """
        Get the current node value as a :class:`uint`.

        :return: If the current node is an unsigned int, its value, 0 otherwise.
        :rtype: :class:`uint`
        """
        return uint(pn_data_get_uint(self._data))

    def get_int(self):
        """
        Get the current node value as a :class:`int32`.

        :return: If the current node is a signed int, its value, 0 otherwise.
        :rtype: :class:`int32`
        """
        return int32(pn_data_get_int(self._data))

    def get_char(self):
        """
        Get the current node value as a :class:`char`.

        :return: If the current node is a char, its value, 0 otherwise.
        :rtype: :class:`char`
        """
        return char(chr(pn_data_get_char(self._data)))

    def get_ulong(self):
        """
        Get the current node value as a :class:`long`.

        :return: If the current node is an unsigned long, its value, 0 otherwise.
        :rtype: :class:`ulong`
        """
        return ulong(pn_data_get_ulong(self._data))

    def get_long(self):
        """
        Get the current node value as a :class:`ulong`.

        :return: If the current node is an signed long, its value, 0 otherwise.
        :rtype: :class:`long`
        """
        return long(pn_data_get_long(self._data))

    def get_timestamp(self):
        """
        Get the current node value as a :class:`timestamp`.

        :return: If the current node is a timestamp, its value, 0 otherwise.
        :rtype: :class:`timestamp`
        """
        return timestamp(pn_data_get_timestamp(self._data))

    def get_float(self):
        """
        Get the current node value as a :class:`float32`.

        :return: If the current node is a float, its value, 0 otherwise.
        :rtype: :class:`float32`
        """
        return float32(pn_data_get_float(self._data))

    def get_double(self):
        """
        Get the current node value as a ``double``.

        :return: If the current node is a double, its value, 0 otherwise.
        :rtype: ``double``
        """
        return pn_data_get_double(self._data)

    # XXX: need to convert
    def get_decimal32(self):
        """
        Get the current node value as a :class:`decimal32`.

        :return: If the current node is a decimal32, its value, 0 otherwise.
        :rtype: :class:`decimal32`
        """
        return decimal32(pn_data_get_decimal32(self._data))

    # XXX: need to convert
    def get_decimal64(self):
        """
        Get the current node value as a :class:`decimal64`.

        :return: If the current node is a decimal64, its value, 0 otherwise.
        :rtype: :class:`decimal64`
        """
        return decimal64(pn_data_get_decimal64(self._data))

    # XXX: need to convert
    def get_decimal128(self):
        """
        Get the current node value as a :class:`decimal128`.

        :return: If the current node is a decimal128, its value, 0 otherwise.
        :rtype: :class:`decimal128`
        """
        return decimal128(pn_data_get_decimal128(self._data))

    def get_uuid(self):
        """
        Get the current node value as a ``uuid.UUID``.

        :return: If the current node is a UUID, its value, ``None`` otherwise.
        :rtype: ``uuid.UUID`` or ``None``
        """
        if pn_data_type(self._data) == Data.UUID:
            return uuid.UUID(bytes=pn_data_get_uuid(self._data))
        else:
            return None

    def get_binary(self):
        """
        Get the current node value as ``bytes``.

        :return: If the current node is binary, its value, ``""`` otherwise.
        :rtype: ``bytes``
        """
        return pn_data_get_binary(self._data)

    def get_string(self):
        """
        Get the current node value as ``str``.

        :return: If the current node is a string, its value, ``""`` otherwise.
        :rtype: ``str``
        """
        return pn_data_get_string(self._data).decode("utf8")

    def get_symbol(self):
        """
        Get the current node value as :class:`symbol`.

        :return: If the current node is a symbol, its value, ``""`` otherwise.
        :rtype: :class:`symbol`
        """
        return symbol(pn_data_get_symbol(self._data).decode('ascii'))

    def copy(self, src):
        """
        Copy the contents of another pn_data_t object. Any values in the
        data object will be lost.

        :param src: The source object from which to copy
        :type src: :class:`Data`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self._check(pn_data_copy(self._data, src._data))

    def format(self):
        """
        Formats the contents of this :class:`Data` object in a human readable way.

        :return: A Formatted string containing contents of this :class:`Data` object.
        :rtype: ``str``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        sz = 16
        while True:
            err, result = pn_data_format(self._data, sz)
            if err == PN_OVERFLOW:
                sz *= 2
                continue
            else:
                self._check(err)
                return result

    def dump(self):
        """
        Dumps a debug representation of the internal state of this :class:`Data`
        object that includes its navigational state to ``cout`` (``stdout``) for
        debugging purposes.
        """
        pn_data_dump(self._data)

    def put_dict(self, d):
        """
        A convenience method for encoding the contents of a Python ``dict``
        as an AMQP map.

        :param d: The dictionary to be encoded
        :type d: ``dict``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self.put_map()
        self.enter()
        try:
            for k, v in d.items():
                self.put_object(k)
                self.put_object(v)
        finally:
            self.exit()

    def get_dict(self):
        """
        A convenience method for decoding an AMQP map as a Python ``dict``.

        :returns: The decoded dictionary.
        :rtype: ``dict``
        """
        if self.enter():
            try:
                result = {}
                while self.next():
                    k = self.get_object()
                    if self.next():
                        v = self.get_object()
                    else:
                        v = None
                    result[k] = v
            finally:
                self.exit()
            return result

    def put_sequence(self, s):
        """
        A convenience method for encoding a Python ``list`` as an
        AMQP list.

        :param s: The sequence to be encoded
        :type s: ``list``
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self.put_list()
        self.enter()
        try:
            for o in s:
                self.put_object(o)
        finally:
            self.exit()

    def get_sequence(self):
        """
        A convenience method for decoding an AMQP list as a Python ``list``.

        :returns: The decoded list.
        :rtype: ``list``
        """
        if self.enter():
            try:
                result = []
                while self.next():
                    result.append(self.get_object())
            finally:
                self.exit()
            return result

    def get_py_described(self):
        """
        A convenience method for decoding an AMQP described value.

        :returns: The decoded AMQP descriptor.
        :rtype: :class:`Described`
        """
        if self.enter():
            try:
                self.next()
                descriptor = self.get_object()
                self.next()
                value = self.get_object()
            finally:
                self.exit()
            return Described(descriptor, value)

    def put_py_described(self, d):
        """
        A convenience method for encoding a :class:`Described` object
        as an AMQP described value. This method encapsulates all the steps
        described in :func:`put_described` into a single method.

        :param d: The descriptor to be encoded
        :type d: :class:`Described`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        self.put_described()
        self.enter()
        try:
            self.put_object(d.descriptor)
            self.put_object(d.value)
        finally:
            self.exit()

    def get_py_array(self):
        """
        A convenience method for decoding an AMQP array into an
        :class:`Array` object. This method encapsulates all the
        steps described in :func:`get_array` into a single function.

        If the current node is an array, return an Array object
        representing the array and its contents. Otherwise return ``None``.

        :returns: The decoded AMQP array.
        :rtype: :class:`Array`
        """

        count, described, type = self.get_array()
        if type is None:
            return None
        if self.enter():
            try:
                if described:
                    self.next()
                    descriptor = self.get_object()
                else:
                    descriptor = UNDESCRIBED
                elements = []
                while self.next():
                    elements.append(self.get_object())
            finally:
                self.exit()
            return Array(descriptor, type, *elements)

    def put_py_array(self, a):
        """
        A convenience method for encoding an :class:`Array` object as
        an AMQP array. This method encapsulates the steps described in
        :func:`put_array` into a single function.

        :param a: The array object to be encoded
        :type a: :class:`Array`
        :raise: :exc:`DataException` if there is a Proton error.
        """
        described = a.descriptor != UNDESCRIBED
        self.put_array(described, a.type)
        self.enter()
        try:
            if described:
                self.put_object(a.descriptor)
            for e in a.elements:
                self.put_object(e)
        finally:
            self.exit()

    put_mappings = {
        None.__class__: lambda s, _: s.put_null(),
        bool: put_bool,
        ubyte: put_ubyte,
        ushort: put_ushort,
        uint: put_uint,
        ulong: put_ulong,
        byte: put_byte,
        short: put_short,
        int32: put_int,
        long: put_long,
        float32: put_float,
        float: put_double,
        decimal32: put_decimal32,
        decimal64: put_decimal64,
        decimal128: put_decimal128,
        char: put_char,
        timestamp: put_timestamp,
        uuid.UUID: put_uuid,
        bytes: put_binary,
        unicode: put_string,
        symbol: put_symbol,
        list: put_sequence,
        tuple: put_sequence,
        dict: put_dict,
        Described: put_py_described,
        Array: put_py_array,
        AnnotationDict: put_dict,
        PropertyDict: put_dict,
        SymbolList: put_sequence,
        memoryview: put_memoryview
    }
    get_mappings = {
        NULL: lambda s: None,
        BOOL: get_bool,
        BYTE: get_byte,
        UBYTE: get_ubyte,
        SHORT: get_short,
        USHORT: get_ushort,
        INT: get_int,
        UINT: get_uint,
        CHAR: get_char,
        LONG: get_long,
        ULONG: get_ulong,
        TIMESTAMP: get_timestamp,
        FLOAT: get_float,
        DOUBLE: get_double,
        DECIMAL32: get_decimal32,
        DECIMAL64: get_decimal64,
        DECIMAL128: get_decimal128,
        UUID: get_uuid,
        BINARY: get_binary,
        STRING: get_string,
        SYMBOL: get_symbol,
        DESCRIBED: get_py_described,
        ARRAY: get_py_array,
        LIST: get_sequence,
        MAP: get_dict
    }

    def put_object(self, obj):
        putter = self.put_mappings[obj.__class__]
        putter(self, obj)

    def get_object(self):
        type = self.type()
        if type is None:
            return None
        getter = self.get_mappings.get(type)
        if getter:
            return getter(self)
        else:
            return UnmappedType(str(type))


def dat2obj(dimpl):
    if dimpl:
        d = Data(dimpl)
        d.rewind()
        d.next()
        obj = d.get_object()
        d.rewind()
        return obj


def obj2dat(obj, dimpl):
    if obj is not None:
        d = Data(dimpl)
        d.put_object(obj)
