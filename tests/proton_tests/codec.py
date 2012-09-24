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

import os, common, xproton
from proton import *

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
