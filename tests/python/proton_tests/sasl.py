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
    self.t2 = Transport()
    self.s2 = SASL(self.t2)

  def pump(self):
    pump(self.t1, self.t2, 1024)

  def testPipelined(self):
    self.s1.mechanisms("ANONYMOUS")
    self.s1.client()

    assert self.s1.outcome is None

    self.s2.mechanisms("ANONYMOUS")
    self.s2.server()
    self.s2.done(SASL.OK)

    out1 = self.t1.output(1024)
    out2 = self.t2.output(1024)

    n = self.t2.input(out1)
    assert n == len(out1), (n, out1)

    assert self.s1.outcome is None

    n = self.t1.input(out2)
    assert n == len(out2), (n, out2)

    assert self.s2.outcome == SASL.OK

  def testSaslAndAmqpInSingleChunk(self):
    self.s1.mechanisms("ANONYMOUS")
    self.s1.client()

    self.s2.mechanisms("ANONYMOUS")
    self.s2.server()
    self.s2.done(SASL.OK)

    # send the server's OK to the client
    out2 = self.t2.output(1024)
    self.t1.input(out2)

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
      out1 = self.t1.output(1024)
      out1_sasl_and_amqp += out1
      t1_still_producing = out1

    t2_still_consuming = True
    while t2_still_consuming:
      num_consumed = self.t2.input(out1_sasl_and_amqp)
      out1_sasl_and_amqp = out1_sasl_and_amqp[num_consumed:]
      t2_still_consuming = num_consumed > 0 and len(out1_sasl_and_amqp) > 0

    assert len(out1_sasl_and_amqp) == 0, (len(out1_sasl_and_amqp), out1_sasl_and_amqp)

    # check that t2 processed both the SASL data and the AMQP data
    assert self.s2.outcome == SASL.OK
    assert c2.state & Endpoint.REMOTE_ACTIVE


  def testChallengeResponse(self):
    self.s1.mechanisms("FAKE_MECH")
    self.s1.client()
    self.s2.mechanisms("FAKE_MECH")
    self.s2.server()
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
    self.s1.client()

    out1 = self.t1.output(1024)
    n = self.t2.input(out1)
    assert n == len(out1)

    self.s2.mechanisms("ANONYMOUS")
    self.s2.server()
    self.s2.done(SASL.OK)
    c2 = Connection()
    c2.open()
    self.t2.bind(c2)

    out2 = self.t2.output(1024)
    n = self.t1.input(out2)
    assert n == len(out2)

    out1 = self.t1.output(1024)
    assert len(out1) > 0

  def testFracturedSASL(self):
    """ PROTON-235
    """
    self.s1.mechanisms("ANONYMOUS")
    self.s1.client()
    assert self.s1.outcome is None

    # self.t1.trace(Transport.TRACE_FRM)

    out = self.t1.output(1024)
    self.t1.input("AMQP\x03\x01\x00\x00")
    out = self.t1.output(1024)
    self.t1.input("\x00\x00\x00")
    out = self.t1.output(1024)
    self.t1.input("A\x02\x01\x00\x00\x00S@\xc04\x01\xe01\x06\xa3\x06GSSAPI\x05PLAIN\x0aDIGEST-MD5\x08AMQPLAIN\x08CRAM-MD5\x04NTLM")
    out = self.t1.output(1024)
    self.t1.input("\x00\x00\x00\x10\x02\x01\x00\x00\x00SD\xc0\x03\x01P\x00")
    out = self.t1.output(1024)
    while out:
      out = self.t1.output(1024)

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
