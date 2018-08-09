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

import sys

from proton import *

from . import common

class Test(common.Test):
  pass

class ClientTransportTest(Test):

  def setUp(self):
    self.transport = Transport()
    self.peer = Transport()
    self.conn = Connection()
    self.peer.bind(self.conn)

  def tearDown(self):
    self.transport = None
    self.peer = None
    self.conn = None

  def drain(self):
    while True:
      p = self.transport.pending()
      if p < 0:
        return
      elif p > 0:
        data = self.transport.peek(p)
        self.peer.push(data)
        self.transport.pop(len(data))
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
    self.transport.push(b"") # should be a noop
    self.transport.close_tail() # should result in framing error
    self.assert_error(u'amqp:connection:framing-error')

  def testPartial(self):
    self.transport.push(b"AMQ") # partial header
    self.transport.close_tail() # should result in framing error
    self.assert_error(u'amqp:connection:framing-error')

  def testGarbage(self, garbage=b"GARBAGE_"):
    self.transport.push(garbage)
    self.assert_error(u'amqp:connection:framing-error')
    assert self.transport.pending() < 0
    self.transport.close_tail()
    assert self.transport.pending() < 0

  def testSmallGarbage(self):
    self.testGarbage(b"XXX")

  def testBigGarbage(self):
    self.testGarbage(b"GARBAGE_XXX")

  def testHeader(self):
    self.transport.push(b"AMQP\x00\x01\x00\x00")
    self.transport.close_tail()
    self.assert_error(u'amqp:connection:framing-error')

  def testHeaderBadDOFF1(self):
    """Verify doff > size error"""
    self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x00\x08\x08\x00\x00\x00")

  def testHeaderBadDOFF2(self):
    """Verify doff < 2 error"""
    self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x00\x08\x01\x00\x00\x00")

  def testHeaderBadSize(self):
    """Verify size > max_frame_size error"""
    self.transport.max_frame_size = 512
    self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x02\x01\x02\x00\x00\x00")

  def testProtocolNotSupported(self):
    self.transport.push(b"AMQP\x01\x01\x0a\x00")
    p = self.transport.pending()
    assert p >= 8, p
    bytes = self.transport.peek(p)
    assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
    self.transport.pop(p)
    self.drain()
    assert self.transport.closed

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
    assert b"test-container" in out, repr(out)
    assert b"test-hostname" in out, repr(out)
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
    except TransportException:
      e = sys.exc_info()[1]
      assert "aborted" in str(e), str(e)
    n = self.transport.pending()
    assert n < 0, n

  def testCloseTail(self):
    n = self.transport.capacity()
    assert n > 0, n
    try:
      self.transport.close_tail()
    except TransportException:
      e = sys.exc_info()[1]
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
    assert self.transport.peek(1024) == b""

    self.peer.push(dat1)
    self.peer.push(dat2[len(dat1):])
    self.peer.push(dat3)

class ServerTransportTest(Test):

  def setUp(self):
    self.transport = Transport(Transport.SERVER)
    self.peer = Transport()
    self.conn = Connection()
    self.peer.bind(self.conn)

  def tearDOwn(self):
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

  # TODO: This may no longer be testing anything
  def testEOS(self):
    self.transport.push(b"") # should be a noop
    self.transport.close_tail()
    p = self.transport.pending()
    self.drain()
    assert self.transport.closed

  def testPartial(self):
    self.transport.push(b"AMQ") # partial header
    self.transport.close_tail()
    p = self.transport.pending()
    assert p >= 8, p
    bytes = self.transport.peek(p)
    assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
    self.transport.pop(p)
    self.drain()
    assert self.transport.closed

  def testGarbage(self, garbage=b"GARBAGE_"):
    self.transport.push(garbage)
    p = self.transport.pending()
    assert p >= 8, p
    bytes = self.transport.peek(p)
    assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
    self.transport.pop(p)
    self.drain()
    assert self.transport.closed

  def testSmallGarbage(self):
    self.testGarbage(b"XXX")

  def testBigGarbage(self):
    self.testGarbage(b"GARBAGE_XXX")

  def testHeader(self):
    self.transport.push(b"AMQP\x00\x01\x00\x00")
    self.transport.close_tail()
    self.assert_error(u'amqp:connection:framing-error')

  def testProtocolNotSupported(self):
    self.transport.push(b"AMQP\x01\x01\x0a\x00")
    p = self.transport.pending()
    assert p >= 8, p
    bytes = self.transport.peek(p)
    assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
    self.transport.pop(p)
    self.drain()
    assert self.transport.closed

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
    assert b"test-container" in out, repr(out)
    assert b"test-hostname" in out, repr(out)
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
    assert n >= 0, n
    try:
      self.transport.close_head()
    except TransportException:
      e = sys.exc_info()[1]
      assert "aborted" in str(e), str(e)
    n = self.transport.pending()
    assert n < 0, n

  def testCloseTail(self):
    n = self.transport.capacity()
    assert n > 0, n
    try:
      self.transport.close_tail()
    except TransportException:
      e = sys.exc_info()[1]
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
    assert self.transport.peek(1024) == b""

    self.peer.push(dat1)
    self.peer.push(dat2[len(dat1):])
    self.peer.push(dat3)

  def testEOSAfterSASL(self):
    self.transport.sasl().allowed_mechs('ANONYMOUS')

    self.peer.sasl().allowed_mechs('ANONYMOUS')

    # this should send over the sasl header plus a sasl-init set up
    # for anonymous
    p = self.peer.pending()
    self.transport.push(self.peer.peek(p))
    self.peer.pop(p)

    # now we send EOS
    self.transport.close_tail()

    # the server may send an error back
    p = self.transport.pending()
    while p>0:
      self.peer.push(self.transport.peek(p))
      self.transport.pop(p)
      p = self.transport.pending()

    # server closed
    assert self.transport.pending() < 0

class LogTest(Test):

  def testTracer(self):
    t = Transport()
    assert t.tracer is None
    messages = []
    def tracer(transport, message):
      messages.append((transport, message))
    t.tracer = tracer
    assert t.tracer is tracer
    t.log("one")
    t.log("two")
    t.log("three")
    assert messages == [(t, "one"), (t, "two"), (t, "three")], messages
