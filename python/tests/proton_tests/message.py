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

from uuid import uuid4

from proton import *

from . import common

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

  def testFirstAcquirer(self):
    self._test("first_acquirer", False, (True, False))

  def testDeliveryCount(self):
    self._test("delivery_count", 0, range(0, 1024))

  def testUserId(self):
    self._test("user_id", b"", (b"asdf", b"fdsa", b"asd\x00fdsa", b""))

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
    self._test("group_sequence", 0, (0, 1, 10, 20, 4294967294, 4294967295))

  def testReplyToGroupId(self):
    self._test_str("reply_to_group_id")

class CodecTest(Test):

  def testProperties(self):
    self.msg.properties = {}
    self.msg.properties['key'] = 'value'
    data = self.msg.encode()

    msg2 = Message()
    msg2.decode(data)

    assert msg2.properties['key'] == 'value', msg2.properties['key']

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

  def testExpiryEncodeAsNull(self):
    self.msg.group_id = "A" # Force creation and expiry fields to be present
    data = self.msg.encode()

    decoder = Data()

    # Skip past the headers
    consumed = decoder.decode(data)
    decoder.clear()
    data = data[consumed:]

    decoder.decode(data)
    dproperties = decoder.get_py_described()
    # Check we've got the correct described list
    assert dproperties.descriptor == 0x73, (dproperties.descriptor)

    properties = dproperties.value
    assert properties[8] == None, properties[8]

  def testCreationEncodeAsNull(self):
    self.msg.group_id = "A" # Force creation and expiry fields to be present
    data = self.msg.encode()

    decoder = Data()

    # Skip past the headers
    consumed = decoder.decode(data)
    decoder.clear()
    data = data[consumed:]

    decoder.decode(data)
    dproperties = decoder.get_py_described()
    # Check we've got the correct described list
    assert dproperties.descriptor == 0x73, (dproperties.descriptor)

    properties = dproperties.value
    assert properties[9] == None, properties[9]

  def testGroupSequenceEncodeAsNull(self):
    self.msg.reply_to_group_id = "R" # Force group_id and group_sequence fields to be present
    data = self.msg.encode()

    decoder = Data()

    # Skip past the headers
    consumed = decoder.decode(data)
    decoder.clear()
    data = data[consumed:]

    decoder.decode(data)
    dproperties = decoder.get_py_described()
    # Check we've got the correct described list
    assert dproperties.descriptor == 0x73, (dproperties.descriptor)

    properties = dproperties.value
    assert properties[10] == None, properties[10]
    assert properties[11] == None, properties[11]

  def testGroupSequenceEncodeAsNonNull(self):
    self.msg.group_id = "G"
    self.msg.reply_to_group_id = "R" # Force group_id and group_sequence fields to be present
    data = self.msg.encode()

    decoder = Data()

    # Skip past the headers
    consumed = decoder.decode(data)
    decoder.clear()
    data = data[consumed:]

    decoder.decode(data)
    dproperties = decoder.get_py_described()
    # Check we've got the correct described list
    assert dproperties.descriptor == 0x73, (dproperties.descriptor)

    properties = dproperties.value
    assert properties[10] == 'G', properties[10]
    assert properties[11] == 0, properties[11]

  def testDefaultCreationExpiryDecode(self):
    # This is a message with everything filled explicitly as null or zero in LIST32 HEADER and PROPERTIES lists
    data = b'\x00\x53\x70\xd0\x00\x00\x00\x0a\x00\x00\x00\x05\x42\x40\x40\x42\x52\x00\x00\x53\x73\xd0\x00\x00\x00\x12\x00\x00\x00\x0d\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x52\x00\x40'
    msg2 = Message()
    msg2.decode(data)
    assert msg2.expiry_time == 0, (msg2.expiry_time)
    assert msg2.creation_time == 0, (msg2.creation_time)

    # The same message with LIST8s instead
    data = b'\x00\x53\x70\xc0\x07\x05\x42\x40\x40\x42\x52\x00\x00\x53\x73\xc0\x0f\x0d\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x52\x00\x40'
    msg3 = Message()
    msg3.decode(data)
    assert msg2.expiry_time == 0, (msg2.expiry_time)
    assert msg2.creation_time == 0, (msg2.creation_time)

    # Minified message with zero length HEADER and PROPERTIES lists
    data = b'\x00\x53\x70\x45' b'\x00\x53\x73\x45'
    msg4 = Message()
    msg4.decode(data)
    assert msg2.expiry_time == 0, (msg2.expiry_time)
    assert msg2.creation_time == 0, (msg2.creation_time)

  def testDefaultPriorityEncode(self):
    assert self.msg.priority == 4, (self.msg.priority)
    self.msg.ttl = 0.003 # field after priority, so forces priority to be present
    data = self.msg.encode()

    decoder = Data()
    decoder.decode(data)

    dheaders = decoder.get_py_described()
    # Check we've got the correct described list
    assert dheaders.descriptor == 0x70, (dheaders.descriptor)

    # Check that the priority field (second field) is encoded as null
    headers = dheaders.value
    assert headers[1] == None, (headers[1])

  def testDefaultPriorityDecode(self):
    # This is a message with everything filled explicitly as null or zero in LIST32 HEADER and PROPERTIES lists
    data = b'\x00\x53\x70\xd0\x00\x00\x00\x0a\x00\x00\x00\x05\x42\x40\x40\x42\x52\x00\x00\x53\x73\xd0\x00\x00\x00\x22\x00\x00\x00\x0d\x40\x40\x40\x40\x40\x40\x40\x40\x83\x00\x00\x00\x00\x00\x00\x00\x00\x83\x00\x00\x00\x00\x00\x00\x00\x00\x40\x52\x00\x40'
    msg2 = Message()
    msg2.decode(data)
    assert msg2.priority == 4, (msg2.priority)

    # The same message with LIST8s instead
    data = b'\x00\x53\x70\xc0\x07\x05\x42\x40\x40\x42\x52\x00\x00\x53\x73\xc0\x1f\x0d\x40\x40\x40\x40\x40\x40\x40\x40\x83\x00\x00\x00\x00\x00\x00\x00\x00\x83\x00\x00\x00\x00\x00\x00\x00\x00\x40\x52\x00\x40'
    msg3 = Message()
    msg3.decode(data)
    assert msg3.priority == 4, (msg3.priority)

    # Minified message with zero length HEADER and PROPERTIES lists
    data = b'\x00\x53\x70\x45' b'\x00\x53\x73\x45'
    msg4 = Message()
    msg4.decode(data)
    assert msg4.priority == 4, (msg4.priority)
