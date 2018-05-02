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

from cproton import PN_TIMESTAMP, PN_FLOAT, PN_DESCRIBED, PN_DECIMAL64, PN_UBYTE, PN_UUID, PN_NULL, PN_BINARY, \
    PN_LIST, PN_OVERFLOW, PN_MAP, PN_LONG, PN_SHORT, PN_CHAR, PN_UINT, PN_ULONG, PN_STRING, PN_USHORT, PN_DOUBLE, \
    PN_BYTE, PN_DECIMAL32, PN_DECIMAL128, PN_ARRAY, PN_SYMBOL, PN_BOOL, PN_INT, \
    pn_data_get_binary, pn_data_get_decimal64, pn_data_put_symbol, pn_data_put_float, \
    pn_data_is_array_described, pn_data_exit, pn_data_put_uint, pn_data_put_decimal128, \
    pn_data_lookup, pn_data_put_char, pn_data_encoded_size, pn_data_get_bool, \
    pn_data_get_short, pn_data_prev, pn_data_type, pn_data_widen, pn_data_put_decimal64, \
    pn_data_put_string, pn_data_get_array, pn_data_put_ulong, pn_data_get_byte, pn_data_get_symbol, pn_data_encode, \
    pn_data_rewind, pn_data_put_bool, pn_data_is_null, pn_data_error, \
    pn_data_put_double, pn_data_copy, pn_data_put_int, pn_data_get_ubyte, pn_data_free, pn_data_clear, \
    pn_data_get_double, pn_data_put_byte, pn_data_put_uuid, pn_data_put_ushort, pn_data_is_described, \
    pn_data_get_float, pn_data_get_uint, pn_data_put_described, pn_data_get_decimal128, pn_data, \
    pn_data_get_array_type, pn_data_put_map, pn_data_put_list, pn_data_get_string, pn_data_get_char, \
    pn_data_put_decimal32, pn_data_enter, pn_data_put_short, pn_data_put_timestamp, \
    pn_data_get_long, pn_data_get_map, pn_data_narrow, pn_data_put_array, pn_data_get_ushort, \
    pn_data_get_int, pn_data_get_list, pn_data_get_ulong, pn_data_put_ubyte, \
    pn_data_format, pn_data_dump, pn_data_get_uuid, pn_data_get_decimal32, \
    pn_data_put_binary, pn_data_get_timestamp, pn_data_decode, pn_data_next, pn_data_put_null, pn_data_put_long, \
    pn_error_text

from ._common import Constant
from ._exceptions import EXCEPTIONS, DataException

from . import _compat

#
# Hacks to provide Python2 <---> Python3 compatibility
#
# The results are
# |       |long|unicode|
# |python2|long|unicode|
# |python3| int|    str|
try:
    long()
except NameError:
    long = int
try:
    unicode()
except NameError:
    unicode = str


class UnmappedType:

    def __init__(self, msg):
        self.msg = msg

    def __repr__(self):
        return "UnmappedType(%s)" % self.msg


class ulong(long):

    def __repr__(self):
        return "ulong(%s)" % long.__repr__(self)


class timestamp(long):

    def __repr__(self):
        return "timestamp(%s)" % long.__repr__(self)


class symbol(unicode):

    def __repr__(self):
        return "symbol(%s)" % unicode.__repr__(self)


class char(unicode):

    def __repr__(self):
        return "char(%s)" % unicode.__repr__(self)


class byte(int):

    def __repr__(self):
        return "byte(%s)" % int.__repr__(self)


class short(int):

    def __repr__(self):
        return "short(%s)" % int.__repr__(self)


class int32(int):

    def __repr__(self):
        return "int32(%s)" % int.__repr__(self)


class ubyte(int):

    def __repr__(self):
        return "ubyte(%s)" % int.__repr__(self)


class ushort(int):

    def __repr__(self):
        return "ushort(%s)" % int.__repr__(self)


class uint(long):

    def __repr__(self):
        return "uint(%s)" % long.__repr__(self)


class float32(float):

    def __repr__(self):
        return "float32(%s)" % float.__repr__(self)


class decimal32(int):

    def __repr__(self):
        return "decimal32(%s)" % int.__repr__(self)


class decimal64(long):

    def __repr__(self):
        return "decimal64(%s)" % long.__repr__(self)


class decimal128(bytes):

    def __repr__(self):
        return "decimal128(%s)" % bytes.__repr__(self)


class Described(object):

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


class Data:
    """
    The L{Data} class provides an interface for decoding, extracting,
    creating, and encoding arbitrary AMQP data. A L{Data} object
    contains a tree of AMQP values. Leaf nodes in this tree correspond
    to scalars in the AMQP type system such as L{ints<INT>} or
    L{strings<STRING>}. Non-leaf nodes in this tree correspond to
    compound values in the AMQP type system such as L{lists<LIST>},
    L{maps<MAP>}, L{arrays<ARRAY>}, or L{described values<DESCRIBED>}.
    The root node of the tree is the L{Data} object itself and can have
    an arbitrary number of children.

    A L{Data} object maintains the notion of the current sibling node
    and a current parent node. Siblings are ordered within their parent.
    Values are accessed and/or added by using the L{next}, L{prev},
    L{enter}, and L{exit} methods to navigate to the desired location in
    the tree and using the supplied variety of put_*/get_* methods to
    access or add a value of the desired type.

    The put_* methods will always add a value I{after} the current node
    in the tree. If the current node has a next sibling the put_* method
    will overwrite the value on this node. If there is no current node
    or the current node has no next sibling then one will be added. The
    put_* methods always set the added/modified node to the current
    node. The get_* methods read the value of the current node and do
    not change which node is current.

    The following types of scalar values are supported:

     - L{NULL}
     - L{BOOL}
     - L{UBYTE}
     - L{USHORT}
     - L{SHORT}
     - L{UINT}
     - L{INT}
     - L{ULONG}
     - L{LONG}
     - L{FLOAT}
     - L{DOUBLE}
     - L{BINARY}
     - L{STRING}
     - L{SYMBOL}

    The following types of compound values are supported:

     - L{DESCRIBED}
     - L{ARRAY}
     - L{LIST}
     - L{MAP}
    """

    NULL = PN_NULL; "A null value."
    BOOL = PN_BOOL; "A boolean value."
    UBYTE = PN_UBYTE; "An unsigned byte value."
    BYTE = PN_BYTE; "A signed byte value."
    USHORT = PN_USHORT; "An unsigned short value."
    SHORT = PN_SHORT; "A short value."
    UINT = PN_UINT; "An unsigned int value."
    INT = PN_INT; "A signed int value."
    CHAR = PN_CHAR; "A character value."
    ULONG = PN_ULONG; "An unsigned long value."
    LONG = PN_LONG; "A signed long value."
    TIMESTAMP = PN_TIMESTAMP; "A timestamp value."
    FLOAT = PN_FLOAT; "A float value."
    DOUBLE = PN_DOUBLE; "A double value."
    DECIMAL32 = PN_DECIMAL32; "A DECIMAL32 value."
    DECIMAL64 = PN_DECIMAL64; "A DECIMAL64 value."
    DECIMAL128 = PN_DECIMAL128; "A DECIMAL128 value."
    UUID = PN_UUID; "A UUID value."
    BINARY = PN_BINARY; "A binary string."
    STRING = PN_STRING; "A unicode string."
    SYMBOL = PN_SYMBOL; "A symbolic string."
    DESCRIBED = PN_DESCRIBED; "A described value."
    ARRAY = PN_ARRAY; "An array value."
    LIST = PN_LIST; "A list value."
    MAP = PN_MAP; "A map value."

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

    @classmethod
    def type_name(type):
        return Data.type_names[type]

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
        unchanged and None is returned.
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
        unchanged and None is returned.
        """
        found = pn_data_prev(self._data)
        if found:
            return self.type()
        else:
            return None

    def enter(self):
        """
        Sets the parent node to the current node and clears the current node.
        Clearing the current node sets it _before_ the first child,
        call next() advances to the first child.
        """
        return pn_data_enter(self._data)

    def exit(self):
        """
        Sets the current node to the parent node and the parent node to
        its own parent.
        """
        return pn_data_exit(self._data)

    def lookup(self, name):
        return pn_data_lookup(self._data, name)

    def narrow(self):
        pn_data_narrow(self._data)

    def widen(self):
        pn_data_widen(self._data)

    def type(self):
        """
        Returns the type of the current node.
        """
        dtype = pn_data_type(self._data)
        if dtype == -1:
            return None
        else:
            return dtype

    def encoded_size(self):
        """
        Returns the size in bytes needed to encode the data in AMQP format.
        """
        return pn_data_encoded_size(self._data)

    def encode(self):
        """
        Returns a representation of the data encoded in AMQP format.
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

        @type encoded: binary
        @param encoded: AMQP encoded binary data
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

        @type described: bool
        @param described: specifies whether the array is described
        @type element_type: int
        @param element_type: the type of the array elements
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
        """
        self._check(pn_data_put_described(self._data))

    def put_null(self):
        """
        Puts a null value.
        """
        self._check(pn_data_put_null(self._data))

    def put_bool(self, b):
        """
        Puts a boolean value.

        @param b: a boolean value
        """
        self._check(pn_data_put_bool(self._data, b))

    def put_ubyte(self, ub):
        """
        Puts an unsigned byte value.

        @param ub: an integral value
        """
        self._check(pn_data_put_ubyte(self._data, ub))

    def put_byte(self, b):
        """
        Puts a signed byte value.

        @param b: an integral value
        """
        self._check(pn_data_put_byte(self._data, b))

    def put_ushort(self, us):
        """
        Puts an unsigned short value.

        @param us: an integral value.
        """
        self._check(pn_data_put_ushort(self._data, us))

    def put_short(self, s):
        """
        Puts a signed short value.

        @param s: an integral value
        """
        self._check(pn_data_put_short(self._data, s))

    def put_uint(self, ui):
        """
        Puts an unsigned int value.

        @param ui: an integral value
        """
        self._check(pn_data_put_uint(self._data, ui))

    def put_int(self, i):
        """
        Puts a signed int value.

        @param i: an integral value
        """
        self._check(pn_data_put_int(self._data, i))

    def put_char(self, c):
        """
        Puts a char value.

        @param c: a single character
        """
        self._check(pn_data_put_char(self._data, ord(c)))

    def put_ulong(self, ul):
        """
        Puts an unsigned long value.

        @param ul: an integral value
        """
        self._check(pn_data_put_ulong(self._data, ul))

    def put_long(self, l):
        """
        Puts a signed long value.

        @param l: an integral value
        """
        self._check(pn_data_put_long(self._data, l))

    def put_timestamp(self, t):
        """
        Puts a timestamp value.

        @param t: an integral value
        """
        self._check(pn_data_put_timestamp(self._data, t))

    def put_float(self, f):
        """
        Puts a float value.

        @param f: a floating point value
        """
        self._check(pn_data_put_float(self._data, f))

    def put_double(self, d):
        """
        Puts a double value.

        @param d: a floating point value.
        """
        self._check(pn_data_put_double(self._data, d))

    def put_decimal32(self, d):
        """
        Puts a decimal32 value.

        @param d: a decimal32 value
        """
        self._check(pn_data_put_decimal32(self._data, d))

    def put_decimal64(self, d):
        """
        Puts a decimal64 value.

        @param d: a decimal64 value
        """
        self._check(pn_data_put_decimal64(self._data, d))

    def put_decimal128(self, d):
        """
        Puts a decimal128 value.

        @param d: a decimal128 value
        """
        self._check(pn_data_put_decimal128(self._data, d))

    def put_uuid(self, u):
        """
        Puts a UUID value.

        @param u: a uuid value
        """
        self._check(pn_data_put_uuid(self._data, u.bytes))

    def put_binary(self, b):
        """
        Puts a binary value.

        @type b: binary
        @param b: a binary value
        """
        self._check(pn_data_put_binary(self._data, b))

    def put_memoryview(self, mv):
        """Put a python memoryview object as an AMQP binary value"""
        self.put_binary(mv.tobytes())

    def put_buffer(self, buff):
        """Put a python buffer object as an AMQP binary value"""
        self.put_binary(bytes(buff))

    def put_string(self, s):
        """
        Puts a unicode value.

        @type s: unicode
        @param s: a unicode value
        """
        self._check(pn_data_put_string(self._data, s.encode("utf8")))

    def put_symbol(self, s):
        """
        Puts a symbolic value.

        @type s: string
        @param s: the symbol name
        """
        self._check(pn_data_put_symbol(self._data, s.encode('ascii')))

    def get_list(self):
        """
        If the current node is a list, return the number of elements,
        otherwise return zero. List elements can be accessed by entering
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
        """
        return pn_data_get_list(self._data)

    def get_map(self):
        """
        If the current node is a map, return the number of child elements,
        otherwise return zero. Key value pairs can be accessed by entering
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
        """
        return pn_data_get_map(self._data)

    def get_array(self):
        """
        If the current node is an array, return a tuple of the element
        count, a boolean indicating whether the array is described, and
        the type of each element, otherwise return (0, False, None). Array
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
        """
        return pn_data_is_described(self._data)

    def is_null(self):
        """
        Checks if the current node is a null.
        """
        return pn_data_is_null(self._data)

    def get_bool(self):
        """
        If the current node is a boolean, returns its value, returns False
        otherwise.
        """
        return pn_data_get_bool(self._data)

    def get_ubyte(self):
        """
        If the current node is an unsigned byte, returns its value,
        returns 0 otherwise.
        """
        return ubyte(pn_data_get_ubyte(self._data))

    def get_byte(self):
        """
        If the current node is a signed byte, returns its value, returns 0
        otherwise.
        """
        return byte(pn_data_get_byte(self._data))

    def get_ushort(self):
        """
        If the current node is an unsigned short, returns its value,
        returns 0 otherwise.
        """
        return ushort(pn_data_get_ushort(self._data))

    def get_short(self):
        """
        If the current node is a signed short, returns its value, returns
        0 otherwise.
        """
        return short(pn_data_get_short(self._data))

    def get_uint(self):
        """
        If the current node is an unsigned int, returns its value, returns
        0 otherwise.
        """
        return uint(pn_data_get_uint(self._data))

    def get_int(self):
        """
        If the current node is a signed int, returns its value, returns 0
        otherwise.
        """
        return int32(pn_data_get_int(self._data))

    def get_char(self):
        """
        If the current node is a char, returns its value, returns 0
        otherwise.
        """
        return char(_compat.unichr(pn_data_get_char(self._data)))

    def get_ulong(self):
        """
        If the current node is an unsigned long, returns its value,
        returns 0 otherwise.
        """
        return ulong(pn_data_get_ulong(self._data))

    def get_long(self):
        """
        If the current node is an signed long, returns its value, returns
        0 otherwise.
        """
        return long(pn_data_get_long(self._data))

    def get_timestamp(self):
        """
        If the current node is a timestamp, returns its value, returns 0
        otherwise.
        """
        return timestamp(pn_data_get_timestamp(self._data))

    def get_float(self):
        """
        If the current node is a float, returns its value, raises 0
        otherwise.
        """
        return float32(pn_data_get_float(self._data))

    def get_double(self):
        """
        If the current node is a double, returns its value, returns 0
        otherwise.
        """
        return pn_data_get_double(self._data)

    # XXX: need to convert
    def get_decimal32(self):
        """
        If the current node is a decimal32, returns its value, returns 0
        otherwise.
        """
        return decimal32(pn_data_get_decimal32(self._data))

    # XXX: need to convert
    def get_decimal64(self):
        """
        If the current node is a decimal64, returns its value, returns 0
        otherwise.
        """
        return decimal64(pn_data_get_decimal64(self._data))

    # XXX: need to convert
    def get_decimal128(self):
        """
        If the current node is a decimal128, returns its value, returns 0
        otherwise.
        """
        return decimal128(pn_data_get_decimal128(self._data))

    def get_uuid(self):
        """
        If the current node is a UUID, returns its value, returns None
        otherwise.
        """
        if pn_data_type(self._data) == Data.UUID:
            return uuid.UUID(bytes=pn_data_get_uuid(self._data))
        else:
            return None

    def get_binary(self):
        """
        If the current node is binary, returns its value, returns ""
        otherwise.
        """
        return pn_data_get_binary(self._data)

    def get_string(self):
        """
        If the current node is a string, returns its value, returns ""
        otherwise.
        """
        return pn_data_get_string(self._data).decode("utf8")

    def get_symbol(self):
        """
        If the current node is a symbol, returns its value, returns ""
        otherwise.
        """
        return symbol(pn_data_get_symbol(self._data).decode('ascii'))

    def copy(self, src):
        self._check(pn_data_copy(self._data, src._data))

    def format(self):
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
        pn_data_dump(self._data)

    def put_dict(self, d):
        self.put_map()
        self.enter()
        try:
            for k, v in d.items():
                self.put_object(k)
                self.put_object(v)
        finally:
            self.exit()

    def get_dict(self):
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
        self.put_list()
        self.enter()
        try:
            for o in s:
                self.put_object(o)
        finally:
            self.exit()

    def get_sequence(self):
        if self.enter():
            try:
                result = []
                while self.next():
                    result.append(self.get_object())
            finally:
                self.exit()
            return result

    def get_py_described(self):
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
        self.put_described()
        self.enter()
        try:
            self.put_object(d.descriptor)
            self.put_object(d.value)
        finally:
            self.exit()

    def get_py_array(self):
        """
        If the current node is an array, return an Array object
        representing the array and its contents. Otherwise return None.
        This is a convenience wrapper around get_array, enter, etc.
        """

        count, described, type = self.get_array()
        if type is None: return None
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
        Array: put_py_array
    }
    # for python 3.x, long is merely an alias for int, but for python 2.x
    # we need to add an explicit int since it is a different type
    if int not in put_mappings:
        put_mappings[int] = put_int
    # Python >=3.0 has 'memoryview', <=2.5 has 'buffer', >=2.6 has both.
    try:
        put_mappings[memoryview] = put_memoryview
    except NameError:
        pass
    try:
        put_mappings[buffer] = put_buffer
    except NameError:
        pass
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
        if type is None: return None
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
