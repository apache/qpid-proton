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
    self.peer = Transport()
    self.conn = Connection()
    self.peer.bind(self.conn)

  def teardown(self):
    self.transport = None
    self.peer = None
    self.conn = None

  def drain(self):
    while True:
      p = self.transport.pending()
      if p < 0:
        return
      elif p > 0:
        bytes = self.transport.peek(p)
        self.peer.push(bytes)
        self.transport.pop(len(bytes))
      else:
        assert False

  def assert_error(self, name):
    assert self.conn.remote_container is None, self.conn.remote_container
    self.drain()
    # verify that we received an open frame
    assert self.conn.remote_container is not None, self.conn.remote_container
    # verify that we received a close frame
    assert self.conn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_CLOSED, self.conn.state
    # verify that a framing error was reported
    assert self.conn.remote_condition.name == name, self.conn.remote_condition

  def testEOS(self):
    self.transport.push("") # should be a noop
    self.transport.close_tail() # should result in framing error
    self.assert_error(u'amqp:connection:framing-error')

  def testPartial(self):
    self.transport.push("AMQ") # partial header
    self.transport.close_tail() # should result in framing error
    self.assert_error(u'amqp:connection:framing-error')

  def testGarbage(self, garbage="GARBAGE_"):
    self.transport.push(garbage)
    self.assert_error(u'amqp:connection:framing-error')
    assert self.transport.pending() < 0
    self.transport.close_tail()
    assert self.transport.pending() < 0

  def testSmallGarbage(self):
    self.testGarbage("XXX")

  def testBigGarbage(self):
    self.testGarbage("GARBAGE_XXX")

  def testHeader(self):
    self.transport.push("AMQP\x00\x01\x00\x00")
    self.transport.close_tail()
    self.assert_error(u'amqp:connection:framing-error')

  def testPeek(self):
    out = self.transport.peek(1024)
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
    out = trn.peek(1024)
    assert "test-container" in out, repr(out)
    assert "test-hostname" in out, repr(out)
    self.transport.push(out)

    c = Connection()
    assert c.remote_container == None
    assert c.remote_hostname == None
    assert c.session_head(0) == None
    self.transport.bind(c)
    assert c.remote_container == "test-container"
    assert c.remote_hostname == "test-hostname"
    assert c.session_head(0) != None

  def testCloseHead(self):
    n = self.transport.pending()
    assert n > 0, n
    try:
      self.transport.close_head()
    except TransportException, e:
      assert "aborted" in str(e), str(e)
    n = self.transport.pending()
    assert n < 0, n

  def testCloseTail(self):
    n = self.transport.capacity()
    assert n > 0, n
    try:
      self.transport.close_tail()
    except TransportException, e:
      assert "aborted" in str(e), str(e)
    n = self.transport.capacity()
    assert n < 0, n

  def testUnpairedPop(self):
    conn = Connection()
    self.transport.bind(conn)

    conn.hostname = "hostname"
    conn.open()

    dat1 = self.transport.peek(1024)

    ssn = conn.session()
    ssn.open()

    dat2 = self.transport.peek(1024)

    assert dat2[:len(dat1)] == dat1

    snd = ssn.sender("sender")
    snd.open()

    self.transport.pop(len(dat1))
    self.transport.pop(len(dat2) - len(dat1))
    dat3 = self.transport.peek(1024)
    self.transport.pop(len(dat3))
    assert self.transport.peek(1024) == ""

    self.peer.push(dat1)
    self.peer.push(dat2[len(dat1):])
    self.peer.push(dat3)
