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

import sys
from uuid import uuid4

from proton import *
from proton._compat import raise_

from . import common

class Test(common.Test):

  def setUp(self):
    self.data = Data()

  def tearDown(self):
    self.data = None

class DataTest(Test):

  def testTopLevelNext(self):
    assert self.data.next() is None
    self.data.put_null()
    self.data.put_bool(False)
    self.data.put_int(0)
    assert self.data.next() is None
    self.data.rewind()
    assert self.data.next() == Data.NULL
    assert self.data.next() == Data.BOOL
    assert self.data.next() == Data.INT
    assert self.data.next() is None

  def testNestedNext(self):
    assert self.data.next() is None
    self.data.put_null()
    assert self.data.next() is None
    self.data.put_list()
    assert self.data.next() is None
    self.data.put_bool(False)
    assert self.data.next() is None
    self.data.rewind()
    assert self.data.next() is Data.NULL
    assert self.data.next() is Data.LIST
    self.data.enter()
    assert self.data.next() is None
    self.data.put_ubyte(0)
    assert self.data.next() is None
    self.data.put_uint(0)
    assert self.data.next() is None
    self.data.put_int(0)
    assert self.data.next() is None
    self.data.exit()
    assert self.data.next() is Data.BOOL
    assert self.data.next() is None

    self.data.rewind()
    assert self.data.next() is Data.NULL
    assert self.data.next() is Data.LIST
    assert self.data.enter()
    assert self.data.next() is Data.UBYTE
    assert self.data.next() is Data.UINT
    assert self.data.next() is Data.INT
    assert self.data.next() is None
    assert self.data.exit()
    assert self.data.next() is Data.BOOL
    assert self.data.next() is None

  def testEnterExit(self):
    assert self.data.next() is None
    assert not self.data.enter()
    self.data.put_list()
    assert self.data.enter()
    assert self.data.next() is None
    self.data.put_list()
    assert self.data.enter()
    self.data.put_list()
    assert self.data.enter()
    assert self.data.exit()
    assert self.data.get_list() == 0
    assert self.data.exit()
    assert self.data.get_list() == 1
    assert self.data.exit()
    assert self.data.get_list() == 1
    assert not self.data.exit()
    assert self.data.get_list() == 1
    assert self.data.next() is None

    self.data.rewind()
    assert self.data.next() is Data.LIST
    assert self.data.get_list() == 1
    assert self.data.enter()
    assert self.data.next() is Data.LIST
    assert self.data.get_list() == 1
    assert self.data.enter()
    assert self.data.next() is Data.LIST
    assert self.data.get_list() == 0
    assert self.data.enter()
    assert self.data.next() is None
    assert self.data.exit()
    assert self.data.get_list() == 0
    assert self.data.exit()
    assert self.data.get_list() == 1
    assert self.data.exit()
    assert self.data.get_list() == 1
    assert not self.data.exit()


  def put(self, putter, v):
    """More informative exception from putters, include bad value"""
    try:
      putter(v)
    except Exception:
      etype, value, trace = sys.exc_info()
      raise_(etype, etype("%s(%r): %s" % (putter.__name__, v, value)), trace)
    return putter

  # (bits, signed) for each integer type
  INT_TYPES = {
    "byte": (8, True),
    "ubyte": (8, False),
    "short": (16, True),
    "ushort": (16, False),
    "int": (32, True),
    "uint": (32, False),
    "long": (64, True),
    "ulong": (64, False)
  }

  def int_values(self, dtype):
    """Set of test values for integer type dtype, include extreme and medial values"""
    bits, signed = self.INT_TYPES[dtype]
    values = [0, 1, 2, 5, 42]
    if signed:
      min, max = -2**(bits-1), 2**(bits-1)-1
      values.append(max // 2)
      values += [-i for i in values if i]
      values += [min, max]
    else:
      max = 2**(bits) - 1
      values += [max // 2, max]
    return sorted(values)

  def _testArray(self, dtype, descriptor, atype, *values):
    if dtype: dTYPE = getattr(self.data, dtype.upper())
    aTYPE = getattr(self.data, atype.upper())
    self.data.put_array(dtype is not None, aTYPE)
    self.data.enter()
    if dtype is not None:
      putter = getattr(self.data, "put_%s" % dtype)
      self.put(putter, descriptor)
    putter = getattr(self.data, "put_%s" % atype)
    for v in values:
      self.put(putter, v)
    self.data.exit()
    self.data.rewind()
    assert self.data.next() == Data.ARRAY
    count, described, type = self.data.get_array()
    assert count == len(values), count
    if dtype is None:
      assert described == False
    else:
      assert described == True
    assert type == aTYPE, type
    assert self.data.enter()
    if described:
      assert self.data.next() == dTYPE
      getter = getattr(self.data, "get_%s" % dtype)
      gotten = getter()
      assert gotten == descriptor, gotten
    if values:
      getter = getattr(self.data, "get_%s" % atype)
      for v in values:
        assert self.data.next() == aTYPE
        gotten = getter()
        assert gotten == v, gotten
    assert self.data.next() is None
    assert self.data.exit()

  def testStringArray(self):
    self._testArray(None, None, "string", "one", "two", "three")

  def testDescribedStringArray(self):
    self._testArray("symbol", "url", "string", "one", "two", "three")

  def _test_int_array(self, atype):
    self._testArray(None, None, atype, *self.int_values(atype))

  def testByteArray(self): self._test_int_array("byte")
  def testUbyteArray(self): self._test_int_array("ubyte")
  def testShortArray(self): self._test_int_array("short")
  def testUshortArray(self): self._test_int_array("ushort")
  def testIntArray(self): self._test_int_array("int")
  def testUintArray(self): self._test_int_array("uint")
  def testLongArray(self): self._test_int_array("long")
  def testUlongArray(self): self._test_int_array("ulong")

  def testUUIDArray(self):
    self._testArray(None, None, "uuid", uuid4(), uuid4(), uuid4())

  def testEmptyArray(self):
    self._testArray(None, None, "null")

  def testDescribedEmptyArray(self):
    self._testArray("long", 0, "null")

  def _test(self, dtype, *values, **kwargs):
    eq=kwargs.get("eq", lambda x, y: x == y)
    ntype = getattr(Data, dtype.upper())
    putter = getattr(self.data, "put_%s" % dtype)
    getter = getattr(self.data, "get_%s" % dtype)

    for v in values:
      self.put(putter, v)
      gotten = getter()
      assert eq(gotten, v), (gotten, v)

    self.data.rewind()

    for v in values:
      vtype = self.data.next()
      assert vtype == ntype, vtype
      gotten = getter()
      assert eq(gotten, v), (gotten, v)

    encoded = self.data.encode()
    copy = Data(0)
    while encoded:
      n = copy.decode(encoded)
      encoded = encoded[n:]
    copy.rewind()

    cgetter = getattr(copy, "get_%s" % dtype)

    for v in values:
      vtype = copy.next()
      assert vtype == ntype, vtype
      gotten = cgetter()
      assert eq(gotten, v), (gotten, v)

  def _test_int(self, itype):
    self._test(itype, *self.int_values(itype))

  def testByte(self): self._test_int("byte")
  def testUbyte(self): self._test_int("ubyte")
  def testShort(self): self._test_int("short")
  def testUshort(self): self._test("ushort")
  def testInt(self): self._test_int("int")
  def testUint(self): self._test_int("uint")
  def testLong(self): self._test_int("long")
  def testUlong(self): self._test_int("ulong")

  def testString(self):
    self._test("string", "one", "two", "three", "this is a test", "")

  def testFloat(self):
    # we have to use a special comparison here because python
    # internally only uses doubles and converting between floats and
    # doubles is imprecise
    self._test("float", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3,
               eq=lambda x, y: x - y < 0.000001)

  def testDouble(self):
    self._test("double", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3)

  def testBinary(self):
    self._test("binary", b"this", b"is", b"a", b"test",b"of" b"b\x00inary")

  def testSymbol(self):
    self._test("symbol", symbol("this is a symbol test"), symbol("bleh"), symbol("blah"))

  def testTimestamp(self):
    self._test("timestamp", timestamp(0), timestamp(12345), timestamp(1000000))

  def testChar(self):
    self._test("char", char('a'), char('b'), char('c'), char(u'\u20AC'))

  def testUUID(self):
    self._test("uuid", uuid4(), uuid4(), uuid4())

  def testDecimal32(self):
    self._test("decimal32", decimal32(0), decimal32(1), decimal32(2), decimal32(3), decimal32(4), decimal32(2**30))

  def testDecimal64(self):
    self._test("decimal64", decimal64(0), decimal64(1), decimal64(2), decimal64(3), decimal64(4), decimal64(2**60))

  def testDecimal128(self):
    self._test("decimal128", decimal128(b"fdsaasdf;lkjjkl;"), decimal128(b"x"*16))

  def testCopy(self):
    self.data.put_described()
    self.data.enter()
    self.data.put_ulong(123)
    self.data.put_map()
    self.data.enter()
    self.data.put_string("pi")
    self.data.put_double(3.14159265359)

    dst = Data()
    dst.copy(self.data)

    copy = dst.format()
    orig = self.data.format()
    assert copy == orig, (copy, orig)

  def testCopyNested(self):
    nested = [1, 2, 3, [4, 5, 6], 7, 8, 9]
    self.data.put_object(nested)
    dst = Data()
    dst.copy(self.data)
    assert dst.format() == self.data.format()

  def testCopyNestedArray(self):
    nested = [Array(UNDESCRIBED, Data.LIST,
                    ["first", [Array(UNDESCRIBED, Data.INT, 1,2,3)]],
                    ["second", [Array(UNDESCRIBED, Data.INT, 1,2,3)]],
                    ["third", [Array(UNDESCRIBED, Data.INT, 1,2,3)]],
                    ),
              "end"]
    self.data.put_object(nested)
    dst = Data()
    dst.copy(self.data)
    assert dst.format() == self.data.format()

  def testRoundTrip(self):
    obj = {symbol("key"): timestamp(1234),
           ulong(123): "blah",
           char("c"): "bleh",
           u"desc": Described(symbol("url"), u"http://example.org"),
           u"array": Array(UNDESCRIBED, Data.INT, 1, 2, 3),
           u"list": [1, 2, 3, None, 4],
           u"boolean": True}
    self.data.put_object(obj)
    enc = self.data.encode()
    data = Data()
    data.decode(enc)
    data.rewind()
    assert data.next()
    copy = data.get_object()
    assert copy == obj, (copy, obj)

  def testBuffer(self):
    try:
      self.data.put_object(buffer(b"foo"))
    except NameError:
      # python >= 3.0 does not have `buffer`
      return
    data = Data()
    data.decode(self.data.encode())
    data.rewind()
    assert data.next()
    assert data.type() == Data.BINARY
    assert data.get_object() == b"foo"

  def testMemoryView(self):
    try:
      self.data.put_object(memoryview(b"foo"))
    except NameError:
      # python <= 2.6 does not have `memoryview`
      return
    data = Data()
    data.decode(self.data.encode())
    data.rewind()
    assert data.next()
    assert data.type() == Data.BINARY
    assert data.get_object() == b"foo"

  def testLookup(self):
    obj = {symbol("key"): u"value",
           symbol("pi"): 3.14159,
           symbol("list"): [1, 2, 3, 4]}
    self.data.put_object(obj)
    self.data.rewind()
    self.data.next()
    self.data.enter()
    self.data.narrow()
    assert self.data.lookup("pi")
    assert self.data.get_object() == 3.14159
    self.data.rewind()
    assert self.data.lookup("key")
    assert self.data.get_object() == u"value"
    self.data.rewind()
    assert self.data.lookup("list")
    assert self.data.get_object() == [1, 2, 3, 4]
    self.data.widen()
    self.data.rewind()
    assert not self.data.lookup("pi")
