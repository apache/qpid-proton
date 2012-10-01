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

# future test areas
#  + different permutations of setup
#   - creating deliveries and calling input/output before opening the session/link
#  + shrinking output_size down to something small? should the enginge buffer?
#  + resuming
#    - locally and remotely created deliveries with the same tag

OUTPUT_SIZE = 10*1024

def pump(t1, t2):
  while True:
    out1 = t1.output(OUTPUT_SIZE)
    out2 = t2.output(OUTPUT_SIZE)

    if out1 or out2:
      if out1:
        n = t2.input(out1)
        assert n is None or n == len(out1), (n, out1, len(out1))
      if out2:
        n = t1.input(out2)
        assert n is None or n == len(out2), (n, out2, len(out2))
    else:
      return

class Test(common.Test):

  def __init__(self, *args):
    common.Test.__init__(self, *args)
    self._wires = []

  def connection(self):
    c1 = Connection()
    c2 = Connection()
    t1 = Transport()
    t1.bind(c1)
    t2 = Transport()
    t2.bind(c2)
    self._wires.append((c1, t1, c2, t2))
    trc = os.environ.get("PN_TRACE_FRM")
    if trc and trc.lower() in ("1", "2", "yes", "true"):
      t1.trace(Transport.TRACE_FRM)
    if trc == "2":
      t2.trace(Transport.TRACE_FRM)
    return c1, c2

  def link(self, name):
    c1, c2 = self.connection()
    c1.open()
    c2.open()
    ssn1 = c1.session()
    ssn1.open()
    self.pump()
    ssn2 = c2.session_head(Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE)
    ssn2.open()
    self.pump()
    snd = ssn1.sender(name)
    rcv = ssn2.receiver(name)
    return snd, rcv

  def cleanup(self):
    pass

  def pump(self):
    for c1, t1, c2, t2 in self._wires:
      pump(t1, t2)

class ConnectionTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert self.c1.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT
    assert self.c2.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.c1.open()
    self.pump()

    assert self.c1.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT
    assert self.c2.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE

    self.c2.open()
    self.pump()

    assert self.c1.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.c1.close()
    self.pump()

    assert self.c1.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    self.c2.close()
    self.pump()

    assert self.c1.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.c2.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

  def test_simultaneous_open_close(self):
    assert self.c1.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT
    assert self.c2.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.c1.open()
    self.c2.open()

    self.pump()

    assert self.c1.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.c1.close()
    self.c2.close()

    self.pump()

    assert self.c1.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.c2.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

class SessionTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()
    self.ssn = self.c1.session()
    self.c1.open()
    self.c2.open()

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert self.ssn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.ssn.open()

    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT

    self.pump()

    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT

    ssn = self.c2.session_head(Endpoint.REMOTE_ACTIVE | Endpoint.LOCAL_UNINIT)

    assert ssn != None
    assert ssn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT

    ssn.open()

    assert ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT

    self.pump()

    assert ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    ssn.close()

    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.pump()

    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    self.ssn.close()

    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

    self.pump()

    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

  def test_simultaneous_close(self):
    self.ssn.open()
    self.pump()
    ssn = self.c2.session_head(Endpoint.REMOTE_ACTIVE | Endpoint.LOCAL_UNINIT)
    assert ssn != None
    ssn.open()
    self.pump()

    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.ssn.close()
    ssn.close()

    assert self.ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE

    self.pump()

    assert self.ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

  def test_closing_connection(self):
    self.ssn.open()
    self.pump()
    self.c1.close()
    self.pump()
    self.ssn.close()
    self.pump()


class LinkTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert self.snd.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.snd.open()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE

    self.rcv.open()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.snd.close()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    self.rcv.close()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.rcv.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

  def test_simultaneous_open_close(self):
    assert self.snd.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_UNINIT

    self.snd.open()
    self.rcv.open()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_UNINIT

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.snd.close()
    self.rcv.close()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.rcv.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

  def test_multiple(self):
    rcv = self.snd.session.receiver("second-rcv")
    self.snd.open()
    rcv.open()
    self.pump()
    c2 = self.rcv.session.connection
    l = c2.link_head(Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE)
    while l:
      l.open()
      l = l.next(Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE)
    self.pump()

    assert self.snd
    assert rcv
    self.snd.close()
    rcv.close()
    ssn = rcv.session
    conn = ssn.connection
    ssn.close()
    conn.close()
    self.pump()

  def test_closing_session(self):
    self.snd.open()
    self.rcv.open()
    ssn1 = self.snd.session
    self.pump()
    ssn1.close()
    self.pump()
    self.snd.close()
    self.pump()

  def test_closing_connection(self):
    self.snd.open()
    self.rcv.open()
    ssn1 = self.snd.session
    c1 = ssn1.connection
    self.pump()
    c1.close()
    self.pump()
    self.snd.close()
    self.pump()

class TransferTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def teardown(self):
    self.cleanup()

  def test_work_queue(self):
    assert self.c1.work_head is None
    self.snd.delivery("tag")
    assert self.c1.work_head is None
    self.rcv.flow(1)
    self.pump()
    d = self.c1.work_head
    assert d is not None
    tag = d.tag
    assert tag == "tag", tag
    assert d.writable

    n = self.snd.send("this is a test")
    assert self.snd.advance()
    assert self.c1.work_head is None

    self.pump()

    d = self.c2.work_head
    assert d.tag == "tag"
    assert d.readable

  def test_multiframe(self):
    self.rcv.flow(1)
    self.snd.delivery("tag")
    msg = "this is a test"
    n = self.snd.send(msg)
    assert n == len(msg)

    self.pump()

    d = self.rcv.current
    assert d
    assert d.tag == "tag", repr(d.tag)
    assert d.readable

    bytes = self.rcv.recv(1024)
    assert bytes == msg

    bytes = self.rcv.recv(1024)
    assert bytes == ""

    msg = "this is more"
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(1024)
    assert bytes == msg

    bytes = self.rcv.recv(1024)
    assert bytes is None

  def test_disposition(self):
    self.rcv.flow(1)

    self.pump()

    sd = self.snd.delivery("tag")
    msg = "this is a test"
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    rd = self.rcv.current
    assert rd is not None
    assert rd.tag == sd.tag
    rmsg = self.rcv.recv(1024)
    assert rmsg == msg
    rd.disposition(Delivery.ACCEPTED)

    self.pump()

    rdisp = sd.remote_disposition
    ldisp = rd.local_disposition
    assert rdisp == ldisp == Delivery.ACCEPTED, (rdisp, ldisp)
    assert sd.updated

    sd.disposition(Delivery.ACCEPTED)
    sd.settle()

    self.pump()

    assert sd.local_disposition == rd.remote_disposition == Delivery.ACCEPTED

class CreditTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def teardown(self):
    self.cleanup()

  def testCreditSender(self):
    credit = self.snd.credit
    assert credit == 0, credit
    self.rcv.flow(10)
    self.pump()
    credit = self.snd.credit
    assert credit == 10, credit

    self.rcv.flow(PN_SESSION_WINDOW)
    self.pump()
    credit = self.snd.credit
    assert credit == 10 + PN_SESSION_WINDOW, credit

  def testCreditReceiver(self):
    self.rcv.flow(10)
    self.pump()
    assert self.rcv.credit == 10, self.rcv.credit

    d = self.snd.delivery("tag")
    assert d
    assert self.snd.advance()
    self.pump()
    assert self.rcv.credit == 10, self.rcv.credit
    assert self.rcv.queued == 1, self.rcv.queued
    c = self.rcv.current
    assert c.tag == "tag", c.tag
    assert self.rcv.advance()
    assert self.rcv.credit == 9, self.rcv.credit
    assert self.rcv.queued == 0, self.rcv.queued

  def settle(self):
    result = []
    d = self.c1.work_head
    while d:
      if d.updated:
        result.append(d.tag)
        d.settle()
      d = d.work_next
    return result

  def testBuffering(self):
    self.rcv.flow(PN_SESSION_WINDOW + 10)
    self.pump()

    assert self.rcv.queued == 0, self.rcv.queued

    idx = 0
    while self.snd.credit:
      d = self.snd.delivery("tag%s" % idx)
      assert d
      assert self.snd.advance()
      self.pump()
      idx += 1

    assert idx == PN_SESSION_WINDOW + 10, idx

    assert self.rcv.queued == PN_SESSION_WINDOW, self.rcv.queued

    extra = self.snd.delivery("extra")
    assert extra
    assert self.snd.advance()
    self.pump()

    assert self.rcv.queued == PN_SESSION_WINDOW, self.rcv.queued

    for i in range(10):
      d = self.rcv.current
      assert d.tag == "tag%s" % i, d.tag
      assert self.rcv.advance()
      d.settle()
      self.pump()
      assert self.rcv.queued == PN_SESSION_WINDOW - (i+1), self.rcv.queued

    tags = self.settle()
    assert tags == ["tag%s" % i for i in range(10)], tags
    self.pump()

    assert self.rcv.queued == PN_SESSION_WINDOW, self.rcv.queued

    for i in range(PN_SESSION_WINDOW):
      d = self.rcv.current
      assert d, i
      assert d.tag == "tag%s" % (i+10), d.tag
      assert self.rcv.advance()
      d.settle()
      self.pump()

    assert self.rcv.queued == 0, self.rcv.queued

    tags = self.settle()
    assert tags == ["tag%s" % (i+10) for i in range(PN_SESSION_WINDOW)]

    assert self.rcv.queued == 0, self.rcv.queued

  def _testBufferingOnClose(self, a, b):
    for i in range(10):
      d = self.snd.delivery("tag-%s" % i)
      assert d
      d.settle()
    self.pump()
    assert self.snd.queued == 10

    endpoints = {"connection": (self.c1, self.c2),
                 "session": (self.snd.session, self.rcv.session),
                 "link": (self.snd, self.rcv)}

    local_a, remote_a = endpoints[a]
    local_b, remote_b = endpoints[b]

    remote_b.close()
    self.pump()
    assert local_b.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED
    local_a.close()
    self.pump()
    assert remote_a.state & Endpoint.REMOTE_CLOSED
    assert self.snd.queued == 10

  def testBufferingOnCloseLinkLink(self):
    self._testBufferingOnClose("link", "link")

  def testBufferingOnCloseLinkSession(self):
    self._testBufferingOnClose("link", "session")

  def testBufferingOnCloseLinkConnection(self):
    self._testBufferingOnClose("link", "connection")

  def testBufferingOnCloseSessionLink(self):
    self._testBufferingOnClose("session", "link")

  def testBufferingOnCloseSessionSession(self):
    self._testBufferingOnClose("session", "session")

  def testBufferingOnCloseSessionConnection(self):
    self._testBufferingOnClose("session", "connection")

  def testBufferingOnCloseConnectionLink(self):
    self._testBufferingOnClose("connection", "link")

  def testBufferingOnCloseConnectionSession(self):
    self._testBufferingOnClose("connection", "session")

  def testBufferingOnCloseConnectionConnection(self):
    self._testBufferingOnClose("connection", "connection")

  def testCreditWithBuffering(self):
    self.rcv.flow(PN_SESSION_WINDOW + 10)
    self.pump()
    assert self.snd.credit == PN_SESSION_WINDOW + 10, self.snd.credit
    assert self.rcv.queued == 0, self.rcv.queued

    idx = 0
    while self.snd.credit:
      d = self.snd.delivery("tag%s" % idx)
      assert d
      assert self.snd.advance()
      self.pump()
      idx += 1

    assert idx == PN_SESSION_WINDOW + 10, idx
    assert self.rcv.queued == PN_SESSION_WINDOW, self.rcv.queued

    self.rcv.flow(1)
    self.pump()
    assert self.snd.credit == 1, self.snd.credit

  def testFullDrain(self):
    assert self.rcv.credit == 0
    assert self.snd.credit == 0
    self.rcv.drain(10)
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10
    self.snd.drained()
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 0
    assert self.snd.credit == 0

  def testPartialDrain(self):
    self.rcv.drain(2)
    self.pump()

    d = self.snd.delivery("tag")
    assert d
    assert self.snd.advance()
    self.snd.drained()
    self.pump()

    c = self.rcv.current
    assert self.rcv.queued == 1, self.rcv.queued
    assert c.tag == d.tag, c.tag
    assert self.rcv.advance()
    assert not self.rcv.current
    assert self.rcv.credit == 0, self.rcv.credit

  def testDrainFlow(self):
    assert self.rcv.credit == 0
    assert self.snd.credit == 0
    self.rcv.drain(10)
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10
    self.snd.drained()
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 0
    assert self.snd.credit == 0
    self.rcv.flow(10)
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10
    self.snd.drained()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10
    self.pump()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10

  def testNegative(self):
    assert self.snd.credit == 0
    d = self.snd.delivery("tag")
    assert d
    assert self.snd.advance()
    self.pump()

    assert self.rcv.credit == 0
    assert self.rcv.queued == 0

    self.rcv.flow(1)
    assert self.rcv.credit == 1
    assert self.rcv.queued == 0
    self.pump()
    assert self.rcv.credit == 1
    assert self.rcv.queued == 1

    c = self.rcv.current
    assert c
    assert c.tag == "tag"
    assert self.rcv.advance()
    assert self.rcv.credit == 0
    assert self.rcv.queued == 0

  def testDrainZero(self):
    assert self.snd.credit == 0
    assert self.rcv.credit == 0
    assert self.rcv.queued == 0

    self.rcv.flow(10)
    self.pump()
    assert self.snd.credit == 10
    assert self.rcv.credit == 10
    assert self.rcv.queued == 0

    self.snd.drained()
    self.pump()
    assert self.snd.credit == 10
    assert self.rcv.credit == 10
    assert self.rcv.queued == 0

    self.rcv.drain(0)
    assert self.snd.credit == 10
    assert self.rcv.credit == 10
    assert self.rcv.queued == 0

    self.pump()

    assert self.snd.credit == 10
    assert self.rcv.credit == 10
    assert self.rcv.queued == 0

    self.snd.drained()
    assert self.snd.credit == 0
    assert self.rcv.credit == 10
    assert self.rcv.queued == 0
    self.pump()

    assert self.snd.credit == 0
    assert self.rcv.credit == 0
    assert self.rcv.queued == 0

class SettlementTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def teardown(self):
    self.cleanup()

  def testSettleCurrent(self):
    self.rcv.flow(10)
    self.pump()

    assert self.snd.credit == 10, self.snd.credit
    d = self.snd.delivery("tag")
    e = self.snd.delivery("tag2")
    assert d
    assert e
    c = self.snd.current
    assert c.tag == "tag", c.tag
    c.settle()
    c = self.snd.current
    assert c.tag == "tag2", c.tag
    c.settle()
    c = self.snd.current
    assert not c
    self.pump()

    c = self.rcv.current
    assert c
    assert c.tag == "tag", c.tag
    assert c.remote_settled
    c.settle()
    c = self.rcv.current
    assert c
    assert c.tag == "tag2", c.tag
    assert c.remote_settled
    c.settle()
    c = self.rcv.current
    assert not c

  def testUnsettled(self):
    self.rcv.flow(10)
    self.pump()

    assert self.snd.unsettled == 0, self.snd.unsettled
    assert self.rcv.unsettled == 0, self.rcv.unsettled

    d = self.snd.delivery("tag")
    assert d
    assert self.snd.unsettled == 1, self.snd.unsettled
    assert self.rcv.unsettled == 0, self.rcv.unsettled
    assert self.snd.advance()
    self.pump()

    assert self.snd.unsettled == 1, self.snd.unsettled
    assert self.rcv.unsettled == 1, self.rcv.unsettled

    c = self.rcv.current
    assert c
    c.settle()

    assert self.snd.unsettled == 1, self.snd.unsettled
    assert self.rcv.unsettled == 0, self.rcv.unsettled

class PipelineTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()

  def teardown(self):
    self.cleanup()

  def test(self):
    ssn = self.c1.session()
    snd = ssn.sender("sender")
    self.c1.open()
    ssn.open()
    snd.open()

    for i in range(10):
      d = snd.delivery("delivery-%s" % i)
      snd.send("delivery-%s" % i)
      d.settle()

    snd.close()
    ssn.close()
    self.c1.close()

    self.pump()

    state = self.c2.state
    assert state == (Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE), "%x" % state
    ssn2 = self.c2.session_head(Endpoint.LOCAL_UNINIT)
    assert ssn2
    state == ssn2.state
    assert state == (Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE), "%x" % state
    rcv = self.c2.link_head(Endpoint.LOCAL_UNINIT)
    assert rcv
    state = rcv.state
    assert state == (Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE), "%x" % state

    self.c2.open()
    ssn2.open()
    rcv.open()
    rcv.flow(10)
    assert rcv.queued == 0, rcv.queued

    self.pump()

    assert rcv.queued == 10, rcv.queued
    state = rcv.state
    assert state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED), "%x" % state
    state = ssn2.state
    assert state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED), "%x" % state
    state = self.c2.state
    assert state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED), "%x" % state

    for i in range(rcv.queued):
      d = rcv.current
      assert d
      assert d.tag == "delivery-%s" % i
      d.settle()

    assert rcv.queued == 0, rcv.queued
