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
from common import pump

class Test(common.Test):
  pass

class SaslTest(Test):

  def setup(self):
    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport(Transport.SERVER)
    self.s2 = SASL(self.t2)

  def pump(self):
    pump(self.t1, self.t2, 1024)

  def testPipelined(self):
    self.s1.mechanisms("ANONYMOUS")

    assert self.s1.outcome is None

    self.s2.mechanisms("ANONYMOUS")
    self.s2.done(SASL.OK)

    out1 = self.t1.peek(1024)
    self.t1.pop(len(out1))
    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))

    self.t2.push(out1)

    assert self.s1.outcome is None

    self.t1.push(out2)

    assert self.s2.outcome == SASL.OK

  def testSaslAndAmqpInSingleChunk(self):
    self.s1.mechanisms("ANONYMOUS")
    self.s1.done(SASL.OK)

    self.s2.mechanisms("ANONYMOUS")
    self.s2.done(SASL.OK)

    # send the server's OK to the client
    # This is still needed for the Java impl
    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))
    self.t1.push(out2)

    # do some work to generate AMQP data
    c1 = Connection()
    c2 = Connection()
    self.t1.bind(c1)
    c1._transport = self.t1
    self.t2.bind(c2)
    c2._transport = self.t2

    c1.open()

    # get all t1's output in one buffer then pass it all to t2
    out1_sasl_and_amqp = ""
    t1_still_producing = True
    while t1_still_producing:
      out1 = self.t1.peek(1024)
      self.t1.pop(len(out1))
      out1_sasl_and_amqp += out1
      t1_still_producing = out1

    t2_still_consuming = True
    while t2_still_consuming:
      num = min(self.t2.capacity(), len(out1_sasl_and_amqp))
      self.t2.push(out1_sasl_and_amqp[:num])
      out1_sasl_and_amqp = out1_sasl_and_amqp[num:]
      t2_still_consuming = num > 0 and len(out1_sasl_and_amqp) > 0

    assert len(out1_sasl_and_amqp) == 0, (len(out1_sasl_and_amqp), out1_sasl_and_amqp)

    # check that t2 processed both the SASL data and the AMQP data
    assert self.s2.outcome == SASL.OK
    assert c2.state & Endpoint.REMOTE_ACTIVE


  def testChallengeResponse(self):
    self.s1.mechanisms("FAKE_MECH")
    self.s2.mechanisms("FAKE_MECH")
    self.pump()
    challenge = "Who goes there!"
    self.s2.send(challenge)
    self.pump()
    ch = self.s1.recv()
    assert ch == challenge, (ch, challenge)

    response = "It is I, Secundus!"
    self.s1.send(response)
    self.pump()
    re = self.s2.recv()
    assert re == response, (re, response)

  def testInitialResponse(self):
    self.s1.plain("secundus", "trustno1")
    self.pump()
    re = self.s2.recv()
    assert re == "\x00secundus\x00trustno1", repr(re)

  def testPipelined2(self):
    self.s1.mechanisms("ANONYMOUS")

    out1 = self.t1.peek(1024)
    self.t1.pop(len(out1))
    self.t2.push(out1)

    self.s2.mechanisms("ANONYMOUS")
    self.s2.done(SASL.OK)
    c2 = Connection()
    c2.open()
    self.t2.bind(c2)

    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))
    self.t1.push(out2)

    out1 = self.t1.peek(1024)
    assert len(out1) > 0

  def testFracturedSASL(self):
    """ PROTON-235
    """
    self.s1.mechanisms("ANONYMOUS")
    assert self.s1.outcome is None

    # self.t1.trace(Transport.TRACE_FRM)

    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push("AMQP\x03\x01\x00\x00")
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push("\x00\x00\x00")
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push("A\x02\x01\x00\x00\x00S@\xc04\x01\xe01\x06\xa3\x06GSSAPI\x05PLAIN\x0aDIGEST-MD5\x08AMQPLAIN\x08CRAM-MD5\x04NTLM")
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push("\x00\x00\x00\x10\x02\x01\x00\x00\x00SD\xc0\x03\x01P\x00")
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    while out:
      out = self.t1.peek(1024)
      self.t1.pop(len(out))

    assert self.s1.outcome == SASL.OK, self.s1.outcome

  def test_singleton(self):
      """Verify that only a single instance of SASL can exist per Transport"""
      transport = Transport()
      sasl1 = SASL(transport)
      sasl2 = transport.sasl()
      sasl3 = SASL(transport)
      assert sasl1 is sasl2
      assert sasl1 is sasl3
      transport = Transport()
      sasl1 = transport.sasl()
      sasl2 = SASL(transport)
      assert sasl1 is sasl2

  def testSaslSkipped(self):
    """Verify that the server (with SASL) correctly handles a client without SASL"""
    self.t1 = Transport()
    self.s2.mechanisms("ANONYMOUS")
    self.s2.allow_skip(True)
    self.pump()
    assert self.s2.outcome == SASL.SKIPPED

