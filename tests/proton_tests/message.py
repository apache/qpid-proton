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
from xproton import *

class Test(common.Test):

  def setup(self):
    self.msg = pn_message()

  def teardown(self):
    pn_message_free(self.msg)
    self.msg = None


class AccessorsTest(Test):

  def _test(self, name, default, values):
    getter = getattr(xproton, "pn_message_get_%s" % name)
    setter = getattr(xproton, "pn_message_set_%s" % name)
    d = getter(self.msg)
    assert d == default, d
    for v in values:
      assert setter(self.msg, v) == 0
      assert getter(self.msg) == v

  def _test_str(self, name):
    self._test(name, None, ("asdf", "fdsa", ""))

  def _test_time(self, name):
    self._test(name, 0, (0, 123456789, 987654321))

  def testDurable(self):
    assert pn_message_is_durable(self.msg) == False
    for v in (True, False):
      assert pn_message_set_durable(self.msg, v) == 0
      assert pn_message_is_durable(self.msg) == v

  def testPriority(self):
    self._test("priority", PN_DEFAULT_PRIORITY, range(0, 255))

  def testTtl(self):
    self._test("ttl", 0, range(12345, 54321))

  def testFirstAquirer(self):
    assert pn_message_is_first_acquirer(self.msg) == False
    for v in (True, False):
      assert pn_message_set_first_acquirer(self.msg, v) == 0
      assert pn_message_is_first_acquirer(self.msg) == v

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
    assert not pn_message_set_ttl(self.msg, 3)
    assert not pn_message_set_priority(self.msg, 100)
    assert not pn_message_set_address(self.msg, "address")
    assert not pn_message_set_subject(self.msg, "subject")
    body = 'Hello World!'
    rc = pn_message_load(self.msg, body)
    assert not rc, rc

    cd, data = pn_message_encode(self.msg, 1024)
    assert cd == 0, cd

    msg2 = pn_message()
    cd = pn_message_decode(msg2, data, len(data))
    assert cd == 0, (cd, data)

    assert pn_message_get_ttl(self.msg) == pn_message_get_ttl(msg2), \
        (pn_message_get_ttl(self.msg), pn_message_get_ttl(msg2))
    assert pn_message_get_priority(self.msg) == pn_message_get_priority(msg2)
    assert pn_message_get_address(self.msg) == pn_message_get_address(msg2), \
        (pn_message_get_address(self.msg), pn_message_get_address(msg2))
    assert pn_message_get_subject(self.msg) == pn_message_get_subject(msg2)
    cd, saved = pn_message_save(self.msg, 1024)
    assert not cd, cd
    assert saved == body, (body, saved)

    pn_message_free(msg2)


class LoadSaveTest(Test):

  def _test(self, fmt, *bodies):
    pn_message_set_format(self.msg, fmt)
    for body in bodies:
      pn_message_clear(self.msg)
      cd, saved = pn_message_save(self.msg, 1024)
      assert  (cd, saved) == (0, ""), (cd, saved)
      err = pn_message_load(self.msg, body)
      assert err == 0, (pn_message_error(self.msg), repr(body))
      cd, saved = pn_message_save(self.msg, 1024)
      assert cd >= 0, (cd, pn_message_error(self.msg))
      assert saved == body, (body, saved)

  def testIntegral(self):
    self._test(PN_AMQP, "0", "1", "-1", "9223372036854775807")

  def testFloating(self):
    self._test(PN_AMQP, "1.1", "3.14159", "-3.14159", "-1.1")

  def testSymbol(self):
    self._test(PN_AMQP, ':symbol', ':"quoted symbol"')

  def testString(self):
    self._test(PN_AMQP, '"string"', '"string with spaces"')

  def testBinary(self):
    self._test(PN_AMQP, 'b"binary"', 'b"binary with spaces and special values: \\x00\\x01\\x02"')

  def testMap(self):
    self._test(PN_AMQP, '{"one"=1, :two=2, :pi=3.14159}', '{[1, 2, 3]=[3, 2, 1], {1=2}={3=4}}')

  def testList(self):
    self._test(PN_AMQP, '[1, 2, 3]', '["one", "two", "three"]', '[:one, 2, 3.14159]',
               '[{1=2}, {3=4}, {5=6}]')

  def testDescriptor(self):
    self._test(PN_AMQP, '@21 ["one", 2, "three", @:url "http://example.org"]')

  def testData(self):
    self._test(PN_DATA, "this is data\x00\x01\x02 blah blah")

  def testText(self):
    self._test(PN_TEXT, "this is a text string")

  def testTextLoadNone(self):
    pn_message_set_format(self.msg, PN_TEXT)
    pn_message_clear(self.msg)
    cd = pn_message_load(self.msg, None)
    assert cd == 0
    cd, saved = pn_message_save(self.msg, 1024)
    assert cd >= 0
    assert saved == ""
