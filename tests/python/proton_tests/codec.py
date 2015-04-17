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

import os, sys
from . import common
from proton import *
try:
  from uuid import uuid4
except ImportError:
  from proton import uuid4
  

class Test(common.Test):

  def setup(self):
    self.data = Data()

  def teardown(self):
    self.data = None

class DataTest(Test):

  def testTopLevelNext(self):
    assert next(self.data) is None
    self.data.put_null()
    self.data.put_bool(False)
    self.data.put_int(0)
    assert next(self.data) is None
    self.data.rewind()
    assert next(self.data) == Data.NULL
    assert next(self.data) == Data.BOOL
    assert next(self.data) == Data.INT
    assert next(self.data) is None

  def testNestedNext(self):
    assert next(self.data) is None
    self.data.put_null()
    assert next(self.data) is None
    self.data.put_list()
    assert next(self.data) is None
    self.data.put_bool(False)
    assert next(self.data) is None
    self.data.rewind()
    assert next(self.data) is Data.NULL
    assert next(self.data) is Data.LIST
    self.data.enter()
    assert next(self.data) is None
    self.data.put_ubyte(0)
    assert next(self.data) is None
    self.data.put_uint(0)
    assert next(self.data) is None
    self.data.put_int(0)
    assert next(self.data) is None
    self.data.exit()
    assert next(self.data) is Data.BOOL
    assert next(self.data) is None

    self.data.rewind()
    assert next(self.data) is Data.NULL
    assert next(self.data) is Data.LIST
    assert self.data.enter()
    assert next(self.data) is Data.UBYTE
    assert next(self.data) is Data.UINT
    assert next(self.data) is Data.INT
    assert next(self.data) is None
    assert self.data.exit()
    assert next(self.data) is Data.BOOL
    assert next(self.data) is None

  def testEnterExit(self):
    assert next(self.data) is None
    assert not self.data.enter()
    self.data.put_list()
    assert self.data.enter()
    assert next(self.data) is None
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
    assert next(self.data) is None

    self.data.rewind()
    assert next(self.data) is Data.LIST
    assert self.data.get_list() == 1
    assert self.data.enter()
    assert next(self.data) is Data.LIST
    assert self.data.get_list() == 1
    assert self.data.enter()
    assert next(self.data) is Data.LIST
    assert self.data.get_list() == 0
    assert self.data.enter()
    assert next(self.data) is None
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
      raise etype, "%s(%r): %s" % (putter.__name__, v, value), trace
    return putter

  # (bits, signed) for each integer type
  INT_TYPES = {
    "byte": (8, True),
    "ubyte": (8, False),
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
      values.append(max / 2)
      values += [-i for i in values if i]
      values += [min, max]
    else:
      max = 2**(bits) - 1
      values += [max / 2, max]
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
    assert next(self.data) == Data.ARRAY
    count, described, type = self.data.get_array()
    assert count == len(values), count
    if dtype is None:
      assert described == False
    else:
      assert described == True
    assert type == aTYPE, type
    assert self.data.enter()
    if described:
      assert next(self.data) == dTYPE
      getter = getattr(self.data, "get_%s" % dtype)
      gotten = getter()
      assert gotten == descriptor, gotten
    if values:
      getter = getattr(self.data, "get_%s" % atype)
      for v in values:
        assert next(self.data) == aTYPE
        gotten = getter()
        assert gotten == v, gotten
    assert next(self.data) is None
    assert self.data.exit()

  def testStringArray(self):
    self._testArray(None, None, "string", "one", "two", "three")

  def testDescribedStringArray(self):
    self._testArray("symbol", "url", "string", "one", "two", "three")

  def _test_int_array(self, atype):
    self._testArray(None, None, atype, *self.int_values(atype))

  def testByteArray(self): self._test_int_array("byte")
  def testUbyteArray(self): self._test_int_array("ubyte")
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
      vtype = next(self.data)
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
      vtype = next(copy)
      assert vtype == ntype, vtype
      gotten = cgetter()
      assert eq(gotten, v), (gotten, v)

  def _test_int(self, itype):
    self._test(itype, *self.int_values(itype))

  def testByte(self): self._test_int("byte")
  def testUbyte(self): self._test_int("ubyte")
  def testInt(self): self._test_int("int")
  def testUint(self): self._test_int("uint")
  def testLong(self): self._test_int("long")
  def testUlong(self): self._test_int("ulong")

  def testString(self):
    self._test("string", "one", "two", "three", "this is a test", "")

  def testFloat(self):
    # we have to use a special comparison here because python
    # internaly only uses doubles and converting between floats and
    # doubles is imprecise
    self._test("float", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3,
               eq=lambda x, y: x - y < 0.000001)

  def testDouble(self):
    self._test("double", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3)

  def testBinary(self):
    self._test("binary", "this", "is", "a", "test", "of" "b\x00inary")

  def testSymbol(self):
    self._test("symbol", "this is a symbol test", "bleh", "blah")

  def testTimestamp(self):
    self._test("timestamp", 0, 12345, 1000000)

  def testChar(self):
    self._test("char", 'a', 'b', 'c', u'\u1234')

  def testUUID(self):
    self._test("uuid", uuid4(), uuid4(), uuid4())

  def testDecimal32(self):
    self._test("decimal32", 0, 1, 2, 3, 4, 2**30)

  def testDecimal64(self):
    self._test("decimal64", 0, 1, 2, 3, 4, 2**60)

  def testDecimal128(self):
    self._test("decimal128", "fdsaasdf;lkjjkl;", "x"*16 )

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
    assert next(data)
    copy = data.get_object()
    assert copy == obj, (copy, obj)

  def testLookup(self):
    obj = {symbol("key"): u"value",
           symbol("pi"): 3.14159,
           symbol("list"): [1, 2, 3, 4]}
    self.data.put_object(obj)
    self.data.rewind()
    next(self.data)
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
