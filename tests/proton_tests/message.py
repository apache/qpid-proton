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
    self.msg = Message()

  def teardown(self):
    self.msg = None


class AccessorsTest(Test):

  def _test(self, name, default, values):
    d = getattr(self.msg, name)
    assert d == default, d
    for v in values:
      setattr(self.msg, name, v)
      gotten = getattr(self.msg, name)
      assert gotten == v, gotten

  def _test_str(self, name):
    self._test(name, None, ("asdf", "fdsa", ""))

  def _test_time(self, name):
    self._test(name, 0, (0, 123456789, 987654321))

  def testId(self):
    self._test("id", None, ("bytes", None, 123, u"string", uuid3(NAMESPACE_OID, "blah")))

  def testCorrelationId(self):
    self._test("correlation_id", None, ("bytes", None, 123, u"string", uuid3(NAMESPACE_OID, "blah")))

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
    self._test("user_id", "", ("asdf", "fdsa", "asd\x00fdsa", ""))

  def testAddress(self):
    self._test_str("address")

  def testSubject(self):
    self._test_str("subject")

  def testReplyTo(self):
    self._test_str("reply_to")

  def testContentType(self):
    self._test_str("content_type")

  def testContentEncoding(self):
    self._test_str("content_encoding")

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
    self.msg.correlation_id = uuid3(NAMESPACE_OID, "bleh")
    self.msg.ttl = 3
    self.msg.priority = 100
    self.msg.address = "address"
    self.msg.subject = "subject"
    body = 'Hello World!'
    self.msg.load(body)

    data = self.msg.encode()

    msg2 = Message()
    msg2.decode(data)

    assert self.msg.id == msg2.id, (self.msg.id, msg2.id)
    assert self.msg.correlation_id == msg2.correlation_id, (self.msg.correlation_id, msg2.correlation_id)
    assert self.msg.ttl == msg2.ttl, (self.msg.ttl, msg2.ttl)
    assert self.msg.priority == msg2.priority, (self.msg.priority, msg2.priority)
    assert self.msg.address == msg2.address, (self.msg.address, msg2.address)
    assert self.msg.subject == msg2.subject, (self.msg.subject, msg2.subject)
    saved = self.msg.save()
    assert saved == body, (body, saved)

class LoadSaveTest(Test):

  def _test(self, fmt, *bodies):
    self.msg.format = fmt
    for body in bodies:
      self.msg.clear()
      saved = self.msg.save()
      assert  saved == "", saved
      self.msg.load(body)
      saved = self.msg.save()
      assert saved == body, (body, saved)

  def testIntegral(self):
    self._test(Message.AMQP, "0", "1", "-1", "9223372036854775807")

  def testFloating(self):
    self._test(Message.AMQP, "1.1", "3.14159", "-3.14159", "-1.1")

  def testSymbol(self):
    self._test(Message.AMQP, ':symbol', ':"quoted symbol"')

  def testString(self):
    self._test(Message.AMQP, '"string"', '"string with spaces"')

  def testBinary(self):
    self._test(Message.AMQP, 'b"binary"', 'b"binary with spaces and special values: \\x00\\x01\\x02"')

  def testMap(self):
    self._test(Message.AMQP, '{"one"=1, :two=2, :pi=3.14159}', '{[1, 2, 3]=[3, 2, 1], {1=2}={3=4}}')

  def testList(self):
    self._test(Message.AMQP, '[1, 2, 3]', '["one", "two", "three"]', '[:one, 2, 3.14159]',
               '[{1=2}, {3=4}, {5=6}]')

  def testDescriptor(self):
    self._test(Message.AMQP, '@21 ["one", 2, "three", @:url "http://example.org"]')

  def testData(self):
    self._test(Message.DATA, "this is data\x00\x01\x02 blah blah")

  def testText(self):
    self._test(Message.TEXT, "this is a text string")

  def testTextLoadNone(self):
    self.msg.format = Message.TEXT
    self.msg.clear()
    self.msg.load(None)
    saved = self.msg.save()
    assert saved == "", repr(saved)
