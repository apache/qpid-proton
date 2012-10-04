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

class Test(common.Test):
  pass

class SaslTest(Test):

  def setup(self):
    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport()
    self.s2 = SASL(self.t2)

  def pump(self):
    while True:
      out1 = self.t1.output(1024)
      out2 = self.t2.output(1024)
      if out1: self.t2.input(out1)
      if out2: self.t1.input(out2)
      if not out1 and not out2: break

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
    assert ch == challenge, ch

    response = "It is I, Secundus!"
    self.s1.send(response)
    self.pump()
    re = self.s2.recv()
    assert re == response, re

  def testInitialResponse(self):
    self.s1.plain("secundus", "trustno1")
    self.pump()
    assert self.s2.recv() == "\x00secundus\x00trustno1"
