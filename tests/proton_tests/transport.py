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

class TransportTest(Test):

  def setup(self):
    self.transport = Transport()

  def teardown(self):
    self.transport = None

  def testEOS(self):
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)

  def testPartial(self):
    n = self.transport.input("AMQ")
    assert n == 3, n
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)

  def testGarbage(self):
    try:
      n = self.transport.input("GARBAGE_")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)

  def testSmallGarbage(self):
    try:
      n = self.transport.input("XXX")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)

  def testBigGarbage(self):
    try:
      n = self.transport.input("GARBAGE_XXX")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "AMQP header mismatch" in str(e)

  def testHeader(self):
    n = self.transport.input("AMQP\x00\x01\x00\x00")
    assert n == 8, n
    try:
      n = self.transport.input("")
      assert False, n
    except TransportException, e:
      assert "connection aborted" in str(e)

  def testOutput(self):
    out = self.transport.output(1024)
    assert out is not None

  def testBindAfterOpen(self):
    conn = Connection()
    ssn = conn.session()
    conn.open()
    ssn.open()
    conn.container = "test-container"
    conn.hostname = "test-hostname"
    trn = Transport()
    trn.bind(conn)
    out = trn.output(1024)
    assert "test-container" in out, repr(out)
    assert "test-hostname" in out, repr(out)
    n = self.transport.input(out)
    assert n > 0, n
    out = out[n:]

    n = self.transport.input(out)
    assert n == 0

    c = Connection()
    assert c.remote_container == None
    assert c.remote_hostname == None
    assert c.session_head(0) == None
    self.transport.bind(c)
    assert c.remote_container == "test-container"
    assert c.remote_hostname == "test-hostname"
    if out:
      assert c.session_head(0) == None
      n = self.transport.input(out)
      assert n == len(out), (n, out)
    assert c.session_head(0) != None
