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

import os
from . import common
from proton import *
from proton._compat import str2bin
try:
  from uuid import uuid4
except ImportError:
  from proton import uuid4

class Test(common.Test):

  def setUp(self):
    self.msg = Message()

  def tearDown(self):
    self.msg = None


class AccessorsTest(Test):

  def _test(self, name, default, values):
    d = getattr(self.msg, name)
    assert d == default, (d, default)
    for v in values:
      setattr(self.msg, name, v)
      gotten = getattr(self.msg, name)
      assert gotten == v, gotten

  def _test_symbol(self, name):
    self._test(name, symbol(None), (symbol(u"abc.123.#$%"), symbol(u"hello.world")))

  def _test_str(self, name):
    self._test(name, None, (u"asdf", u"fdsa", u""))

  def _test_time(self, name):
    self._test(name, 0, (0, 123456789, 987654321))

  def testId(self):
    self._test("id", None, ("bytes", None, 123, u"string", uuid4()))

  def testCorrelationId(self):
    self._test("correlation_id", None, ("bytes", None, 123, u"string", uuid4()))

  def testDurable(self):
    self._test("durable", False, (True, False))

  def testPriority(self):
    self._test("priority", Message.DEFAULT_PRIORITY, range(0, 255))

  def testTtl(self):
    self._test("ttl", 0, range(12345, 54321))

  def testFirstAquirer(self):
    self._test("first_acquirer", False, (True, False))

  def testDeliveryCount(self):
    self._test("delivery_count", 0, range(0, 1024))

  def testUserId(self):
    self._test("user_id", str2bin(""), (str2bin("asdf"), str2bin("fdsa"),
                                      str2bin("asd\x00fdsa"), str2bin("")))

  def testAddress(self):
    self._test_str("address")

  def testSubject(self):
    self._test_str("subject")

  def testReplyTo(self):
    self._test_str("reply_to")

  def testContentType(self):
    self._test_symbol("content_type")

  def testContentEncoding(self):
    self._test_symbol("content_encoding")

  def testExpiryTime(self):
    self._test_time("expiry_time")

  def testCreationTime(self):
    self._test_time("creation_time")

  def testGroupId(self):
    self._test_str("group_id")

  def testGroupSequence(self):
    self._test("group_sequence", 0, (0, -10, 10, 20, -20))

  def testReplyToGroupId(self):
    self._test_str("reply_to_group_id")

class CodecTest(Test):

  def testRoundTrip(self):
    self.msg.id = "asdf"
    self.msg.correlation_id = uuid4()
    self.msg.ttl = 3
    self.msg.priority = 100
    self.msg.address = "address"
    self.msg.subject = "subject"
    self.msg.body = 'Hello World!'

    data = self.msg.encode()

    msg2 = Message()
    msg2.decode(data)

    assert self.msg.id == msg2.id, (self.msg.id, msg2.id)
    assert self.msg.correlation_id == msg2.correlation_id, (self.msg.correlation_id, msg2.correlation_id)
    assert self.msg.ttl == msg2.ttl, (self.msg.ttl, msg2.ttl)
    assert self.msg.priority == msg2.priority, (self.msg.priority, msg2.priority)
    assert self.msg.address == msg2.address, (self.msg.address, msg2.address)
    assert self.msg.subject == msg2.subject, (self.msg.subject, msg2.subject)
    assert self.msg.body == msg2.body, (self.msg.body, msg2.body)
