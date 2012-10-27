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

import os, common
from proton import *
from uuid import uuid3, NAMESPACE_OID

class Test(common.Test):

  def setup(self):
    self.data = Data()

  def teardown(self):
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

  def _testArray(self, dtype, descriptor, atype, *values):
    if dtype: dTYPE = getattr(self.data, dtype.upper())
    aTYPE = getattr(self.data, atype.upper())
    self.data.put_array(dtype is not None, aTYPE)
    self.data.enter()
    if dtype is not None:
      putter = getattr(self.data, "put_%s" % dtype)
      putter(descriptor)
    putter = getattr(self.data, "put_%s" % atype)
    for v in values:
      putter(v)
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

  def testIntArray(self):
    self._testArray(None, None, "int", 1, 2, 3)

  def testUUIDArray(self):
    self._testArray(None, None, "uuid", uuid3(NAMESPACE_OID, "one"),
                    uuid3(NAMESPACE_OID, "one"),
                    uuid3(NAMESPACE_OID, "three"))

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
      putter(v)
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

  def testInt(self):
    self._test("int", 1, 2, 3, -1, -2, -3)

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
    self._test("uuid", uuid3(NAMESPACE_OID, "test1"), uuid3(NAMESPACE_OID, "test2"),
               uuid3(NAMESPACE_OID, "test3"))

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

    assert dst.format() == self.data.format()

  def testRoundTrip(self):
    obj = {symbol("key"): timestamp(1234),
           ulong(123): "blah",
           char("c"): "bleh",
           u"desc": Described(symbol("url"), u"http://example.org"),
           u"array": Array(UNDESCRIBED, Data.INT, 1, 2, 3),
           u"boolean": True}
    self.data.put_object(obj)
    enc = self.data.encode()
    data = Data()
    data.decode(enc)
    data.rewind()
    assert data.next()
    copy = data.get_object()
    assert copy == obj, (copy, obj)
