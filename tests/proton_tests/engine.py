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

# future test areas
#  + different permutations of setup
#   - creating deliveries and calling input/output before opening the session/link
#  + shrinking output_size down to something small? should the enginge buffer?
#  + resuming
#    - locally and remotely created deliveries with the same tag

OUTPUT_SIZE = 10*1024

def pump(t1, t2):
  while True:
    cd, out1 = pn_output(t1, OUTPUT_SIZE)
    assert cd >= 0 or cd == PN_EOS, (cd, out1, len(out1))
    cd, out2 = pn_output(t2, OUTPUT_SIZE)
    assert cd >= 0 or cd == PN_EOS, (cd, out2, len(out2))

    if out1 or out2:
      if out1:
        cd = pn_input(t2, out1)
        assert cd == PN_EOS or cd == len(out1), \
            (cd, out1, len(out1), pn_error_text(pn_transport_error(t2)))
      if out2:
        cd = pn_input(t1, out2)
        assert cd == PN_EOS or cd == len(out2), \
            (cd, out2, len(out2), pn_error_text(pn_transport_error(t1)))
    else:
      return

class Test(common.Test):

  def __init__(self, *args):
    common.Test.__init__(self, *args)
    self._wires = []

  def connection(self):
    c1 = pn_connection()
    c2 = pn_connection()
    t1 = pn_transport()
    pn_transport_bind(t1, c1)
    t2 = pn_transport()
    pn_transport_bind(t2, c2)
    self._wires.append((c1, t1, c2, t2))
    trc = os.environ.get("PN_TRACE_FRM")
    if trc and trc.lower() in ("1", "2", "yes", "true"):
      pn_trace(t1, PN_TRACE_FRM)
    if trc == "2":
      pn_trace(t2, PN_TRACE_FRM)
    return c1, c2

  def link(self, name):
    c1, c2 = self.connection()
    pn_connection_open(c1)
    pn_connection_open(c2)
    ssn1 = pn_session(c1)
    pn_session_open(ssn1)
    self.pump()
    ssn2 = pn_session_head(c2, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    pn_session_open(ssn2)
    self.pump()
    snd = pn_sender(ssn1, name)
    rcv = pn_receiver(ssn2, name)
    return snd, rcv

  def cleanup(self):
    for c1, t1, c2, t2 in self._wires:
      pn_connection_free(c1)
      pn_transport_free(t1)
      pn_connection_free(c2)
      pn_transport_free(t2)

  def pump(self):
    for c1, t1, c2, t2 in self._wires:
      pump(t1, t2)

class ConnectionTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert pn_connection_state(self.c1) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT
    assert pn_connection_state(self.c2) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    pn_connection_open(self.c1)
    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT
    assert pn_connection_state(self.c2) == PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE

    pn_connection_open(self.c2)
    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_connection_state(self.c2) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_connection_close(self.c1)
    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_connection_state(self.c2) == PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED

    pn_connection_close(self.c2)
    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_connection_state(self.c2) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

  def test_simultaneous_open_close(self):
    assert pn_connection_state(self.c1) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT
    assert pn_connection_state(self.c2) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    pn_connection_open(self.c1)
    pn_connection_open(self.c2)

    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_connection_state(self.c2) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_connection_close(self.c1)
    pn_connection_close(self.c2)

    self.pump()

    assert pn_connection_state(self.c1) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_connection_state(self.c2) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

class SessionTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()
    self.ssn = pn_session(self.c1)
    pn_connection_open(self.c1)
    pn_connection_open(self.c2)

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert pn_session_state(self.ssn) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    pn_session_open(self.ssn)

    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT

    self.pump()

    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT

    ssn = pn_session_head(self.c2, PN_REMOTE_ACTIVE | PN_LOCAL_UNINIT)

    assert ssn != None
    assert pn_session_state(ssn) == PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT

    pn_session_open(ssn)

    assert pn_session_state(ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT

    self.pump()

    assert pn_session_state(ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_session_close(ssn)

    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    self.pump()

    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED

    pn_session_close(self.ssn)

    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_session_state(self.ssn) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

    self.pump()

    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_session_state(self.ssn) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

  def test_simultaneous_close(self):
    pn_session_open(self.ssn)
    self.pump()
    ssn = pn_session_head(self.c2, PN_REMOTE_ACTIVE | PN_LOCAL_UNINIT)
    assert ssn != None
    pn_session_open(ssn)
    self.pump()

    assert pn_session_state(self.ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_session_state(ssn) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_session_close(self.ssn)
    pn_session_close(ssn)

    assert pn_session_state(self.ssn) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE

    self.pump()

    assert pn_session_state(self.ssn) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_session_state(ssn) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

  def test_closing_connection(self):
    pn_session_open(self.ssn)
    self.pump()
    pn_connection_close(self.c1)
    self.pump()
    pn_session_close(self.ssn)
    self.pump()


class LinkTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")

  def teardown(self):
    self.cleanup()

  def test_open_close(self):
    assert pn_link_state(self.snd) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    pn_link_open(self.snd)

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE

    pn_link_open(self.rcv)

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_link_close(self.snd)

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED

    pn_link_close(self.rcv)

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_link_state(self.rcv) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

  def test_simultaneous_open_close(self):
    assert pn_link_state(self.snd) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_UNINIT | PN_REMOTE_UNINIT

    pn_link_open(self.snd)
    pn_link_open(self.rcv)

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_UNINIT

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_ACTIVE | PN_REMOTE_ACTIVE

    pn_link_close(self.snd)
    pn_link_close(self.rcv)

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE
    assert pn_link_state(self.rcv) == PN_LOCAL_CLOSED | PN_REMOTE_ACTIVE

    self.pump()

    assert pn_link_state(self.snd) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED
    assert pn_link_state(self.rcv) == PN_LOCAL_CLOSED | PN_REMOTE_CLOSED

  def test_multiple(self):
    rcv = pn_receiver(pn_get_session(self.snd), "second-rcv")
    pn_link_open(self.snd)
    pn_link_open(rcv)
    self.pump()
    c2 = pn_get_connection(pn_get_session(self.rcv))
    l = pn_link_head(c2, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    while l:
      pn_link_open(l)
      l = pn_link_next(l, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    self.pump()

    assert self.snd
    assert rcv
    pn_link_close(self.snd)
    pn_link_close(rcv)
    ssn = pn_get_session(rcv)
    conn = pn_get_connection(ssn)
    pn_session_close(ssn)
    pn_connection_close(conn)
    self.pump()

  def test_closing_session(self):
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    ssn1 = pn_get_session(self.snd)
    self.pump()
    pn_session_close(ssn1)
    self.pump()
    pn_link_close(self.snd)
    self.pump()

  def test_closing_connection(self):
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    ssn1 = pn_get_session(self.snd)
    c1 = pn_get_connection(ssn1)
    self.pump()
    pn_connection_close(c1)
    self.pump()
    pn_link_close(self.snd)
    self.pump()

class TransferTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = pn_get_connection(pn_get_session(self.snd))
    self.c2 = pn_get_connection(pn_get_session(self.rcv))
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    self.pump()

  def teardown(self):
    self.cleanup()

  def test_work_queue(self):
    assert pn_work_head(self.c1) is None
    pn_delivery(self.snd, "tag")
    assert pn_work_head(self.c1) is None
    pn_flow(self.rcv, 1)
    self.pump()
    d = pn_work_head(self.c1)
    assert d is not None
    tag = pn_delivery_tag(d)
    assert tag == "tag", tag
    assert pn_writable(d)

    n = pn_send(self.snd, "this is a test")
    assert pn_advance(self.snd)
    assert pn_work_head(self.c1) is None

    self.pump()

    d = pn_work_head(self.c2)
    assert pn_delivery_tag(d) == "tag"
    assert pn_readable(d)

  def test_multiframe(self):
    pn_flow(self.rcv, 1)
    pn_delivery(self.snd, "tag")
    msg = "this is a test"
    n = pn_send(self.snd, msg)
    assert n == len(msg)

    self.pump()

    d = pn_current(self.rcv)
    assert d
    assert pn_delivery_tag(d) == "tag", repr(pn_delivery_tag(d))
    assert pn_readable(d)

    cd, bytes = pn_recv(self.rcv, 1024)
    assert bytes == msg
    assert cd == len(bytes)

    cd, bytes = pn_recv(self.rcv, 1024)
    assert cd == 0
    assert bytes == ""

    msg = "this is more"
    n = pn_send(self.snd, msg)
    assert n == len(msg)
    assert pn_advance(self.snd)

    self.pump()

    cd, bytes = pn_recv(self.rcv, 1024)
    assert cd == len(bytes)
    assert bytes == msg

    cd, bytes = pn_recv(self.rcv, 1024)
    assert cd == PN_EOS
    assert bytes == ""

  def test_disposition(self):
    pn_flow(self.rcv, 1)

    self.pump()

    sd = pn_delivery(self.snd, "tag")
    msg = "this is a test"
    n = pn_send(self.snd, msg)
    assert n == len(msg)
    assert pn_advance(self.snd)

    self.pump()

    rd = pn_current(self.rcv)
    assert rd is not None
    assert pn_delivery_tag(rd) == pn_delivery_tag(sd)
    cd, rmsg = pn_recv(self.rcv, 1024)
    assert cd == len(rmsg)
    assert rmsg == msg
    pn_disposition(rd, PN_ACCEPTED)

    self.pump()

    rdisp = pn_remote_disposition(sd)
    ldisp = pn_local_disposition(rd)
    assert rdisp == ldisp == PN_ACCEPTED, (rdisp, ldisp)
    assert pn_updated(sd)

    pn_disposition(sd, PN_ACCEPTED)
    pn_settle(sd)

    self.pump()

    assert pn_local_disposition(sd) == pn_remote_disposition(rd) == PN_ACCEPTED

class CreditTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = pn_get_connection(pn_get_session(self.snd))
    self.c2 = pn_get_connection(pn_get_session(self.rcv))
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    self.pump()

  def teardown(self):
    self.cleanup()

  def testCreditSender(self):
    credit = pn_credit(self.snd)
    assert credit == 0, credit
    pn_flow(self.rcv, 10)
    self.pump()
    credit = pn_credit(self.snd)
    assert credit == 10, credit

    pn_flow(self.rcv, PN_SESSION_WINDOW)
    self.pump()
    credit = pn_credit(self.snd)
    assert credit == 10 + PN_SESSION_WINDOW, credit

  def testCreditReceiver(self):
    pn_flow(self.rcv, 10)
    self.pump()
    assert pn_credit(self.rcv) == 10, pn_credit(self.rcv)

    d = pn_delivery(self.snd, "tag")
    assert d
    assert pn_advance(self.snd)
    self.pump()
    assert pn_credit(self.rcv) == 10, pn_credit(self.rcv)
    assert pn_queued(self.rcv) == 1, pn_queued(self.rcv)
    c = pn_current(self.rcv)
    assert pn_delivery_tag(c) == "tag", pn_delivery_tag(c)
    assert pn_advance(self.rcv)
    assert pn_credit(self.rcv) == 9, pn_credit(self.rcv)
    assert pn_queued(self.rcv) == 0, pn_queued(self.rcv)

  def settle(self):
    result = []
    d = pn_work_head(self.c1)
    while d:
      if pn_updated(d):
        result.append(pn_delivery_tag(d))
        pn_settle(d)
      d = pn_work_next(d)
    return result

  def testBuffering(self):
    pn_flow(self.rcv, PN_SESSION_WINDOW + 10)
    self.pump()

    assert pn_queued(self.rcv) == 0, pn_queued(self.rcv)

    idx = 0
    while pn_credit(self.snd):
      d = pn_delivery(self.snd, "tag%s" % idx)
      assert d
      assert pn_advance(self.snd)
      self.pump()
      idx += 1

    assert idx == PN_SESSION_WINDOW + 10, idx

    assert pn_queued(self.rcv) == PN_SESSION_WINDOW, pn_queued(self.rcv)

    extra = pn_delivery(self.snd, "extra")
    assert extra
    assert pn_advance(self.snd)
    self.pump()

    assert pn_queued(self.rcv) == PN_SESSION_WINDOW, pn_queued(self.rcv)

    for i in range(10):
      d = pn_current(self.rcv)
      assert pn_delivery_tag(d) == "tag%s" % i, pn_delivery_tag(d)
      assert pn_advance(self.rcv)
      pn_settle(d)
      self.pump()
      assert pn_queued(self.rcv) == PN_SESSION_WINDOW - (i+1), pn_queued(self.rcv)

    tags = self.settle()
    assert tags == ["tag%s" % i for i in range(10)], tags
    self.pump()

    assert pn_queued(self.rcv) == PN_SESSION_WINDOW, pn_queued(self.rcv)

    for i in range(PN_SESSION_WINDOW):
      d = pn_current(self.rcv)
      assert d, i
      assert pn_delivery_tag(d) == "tag%s" % (i+10), pn_delivery_tag(d)
      assert pn_advance(self.rcv)
      pn_settle(d)
      self.pump()

    assert pn_queued(self.rcv) == 0, pn_queued(self.rcv)

    tags = self.settle()
    assert tags == ["tag%s" % (i+10) for i in range(PN_SESSION_WINDOW)]

    assert pn_queued(self.rcv) == 0, pn_queued(self.rcv)

  def _testBufferingOnClose(self, a, b):
    for i in range(10):
      d = pn_delivery(self.snd, "tag-%s" % i)
      assert d
      pn_settle(d)
    self.pump()
    assert pn_queued(self.snd) == 10

    endpoints = {"connection": (self.c1, self.c2),
                 "session": (pn_get_session(self.snd), pn_get_session(self.rcv)),
                 "link": (self.snd, self.rcv)}

    local_a, remote_a = endpoints[a]
    local_b, remote_b = endpoints[b]

    a_close = getattr(xproton, "pn_%s_close" % a)
    a_state = getattr(xproton, "pn_%s_state" % a)
    b_close = getattr(xproton, "pn_%s_close" % b)
    b_state = getattr(xproton, "pn_%s_state" % b)

    b_close(remote_b)
    self.pump()
    assert b_state(local_b) == PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED
    a_close(local_a)
    self.pump()
    assert a_state(remote_a) & PN_REMOTE_CLOSED
    assert pn_queued(self.snd) == 10

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
    pn_flow(self.rcv, PN_SESSION_WINDOW + 10)
    self.pump()
    assert pn_credit(self.snd) == PN_SESSION_WINDOW + 10, pn_credit(self.snd)
    assert pn_queued(self.rcv) == 0, pn_queued(self.rcv)

    idx = 0
    while pn_credit(self.snd):
      d = pn_delivery(self.snd, "tag%s" % idx)
      assert d
      assert pn_advance(self.snd)
      self.pump()
      idx += 1

    assert idx == PN_SESSION_WINDOW + 10, idx
    assert pn_queued(self.rcv) == PN_SESSION_WINDOW, pn_queued(self.rcv)

    pn_flow(self.rcv, 1)
    self.pump()
    assert pn_credit(self.snd) == 1, pn_credit(self.snd)

  def testFullDrain(self):
    assert pn_credit(self.rcv) == 0
    assert pn_credit(self.snd) == 0
    pn_drain(self.rcv, 10)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 0
    self.pump()
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 10
    pn_drained(self.snd)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 0
    self.pump()
    assert pn_credit(self.rcv) == 0
    assert pn_credit(self.snd) == 0

  def testPartialDrain(self):
    pn_drain(self.rcv, 2)
    self.pump()

    d = pn_delivery(self.snd, "tag")
    assert d
    assert pn_advance(self.snd)
    pn_drained(self.snd)
    self.pump()

    c = pn_current(self.rcv)
    assert pn_queued(self.rcv) == 1, pn_queued(self.rcv)
    assert pn_delivery_tag(c) == pn_delivery_tag(d), pn_delivery_tag(c)
    assert pn_advance(self.rcv)
    assert not pn_current(self.rcv)
    assert pn_credit(self.rcv) == 0, pn_credit(self.rcv)

  def testDrainFlow(self):
    assert pn_credit(self.rcv) == 0
    assert pn_credit(self.snd) == 0
    pn_drain(self.rcv, 10)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 0
    self.pump()
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 10
    pn_drained(self.snd)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 0
    self.pump()
    assert pn_credit(self.rcv) == 0
    assert pn_credit(self.snd) == 0
    pn_flow(self.rcv, 10)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 0
    self.pump()
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 10
    pn_drained(self.snd)
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 10
    self.pump()
    assert pn_credit(self.rcv) == 10
    assert pn_credit(self.snd) == 10

  def testNegative(self):
    assert pn_credit(self.snd) == 0
    d = pn_delivery(self.snd, "tag")
    assert d
    assert pn_advance(self.snd)
    self.pump()

    assert pn_credit(self.rcv) == 0
    assert pn_queued(self.rcv) == 0

    pn_flow(self.rcv, 1)
    assert pn_credit(self.rcv) == 1
    assert pn_queued(self.rcv) == 0
    self.pump()
    assert pn_credit(self.rcv) == 1
    assert pn_queued(self.rcv) == 1

    c = pn_current(self.rcv)
    assert c
    assert pn_delivery_tag(c) == "tag"
    assert pn_advance(self.rcv)
    assert pn_credit(self.rcv) == 0
    assert pn_queued(self.rcv) == 0

  def testDrainZero(self):
    assert pn_credit(self.snd) == 0
    assert pn_credit(self.rcv) == 0
    assert pn_queued(self.rcv) == 0

    pn_flow(self.rcv, 10)
    self.pump()
    assert pn_credit(self.snd) == 10
    assert pn_credit(self.rcv) == 10
    assert pn_queued(self.rcv) == 0

    pn_drained(self.snd)
    self.pump()
    assert pn_credit(self.snd) == 10
    assert pn_credit(self.rcv) == 10
    assert pn_queued(self.rcv) == 0

    pn_drain(self.rcv, 0)
    assert pn_credit(self.snd) == 10
    assert pn_credit(self.rcv) == 10
    assert pn_queued(self.rcv) == 0

    self.pump()

    assert pn_credit(self.snd) == 10
    assert pn_credit(self.rcv) == 10
    assert pn_queued(self.rcv) == 0

    pn_drained(self.snd)
    assert pn_credit(self.snd) == 0
    assert pn_credit(self.rcv) == 10
    assert pn_queued(self.rcv) == 0
    self.pump()

    assert pn_credit(self.snd) == 0
    assert pn_credit(self.rcv) == 0
    assert pn_queued(self.rcv) == 0

class SettlementTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = pn_get_connection(pn_get_session(self.snd))
    self.c2 = pn_get_connection(pn_get_session(self.rcv))
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    self.pump()

  def teardown(self):
    self.cleanup()

  def testSettleCurrent(self):
    pn_flow(self.rcv, 10)
    self.pump()

    assert pn_credit(self.snd) == 10, pn_credit(self.snd)
    d = pn_delivery(self.snd, "tag")
    e = pn_delivery(self.snd, "tag2")
    assert d
    assert e
    c = pn_current(self.snd)
    assert pn_delivery_tag(c) == "tag", pn_delivery_tag(c)
    pn_settle(c)
    c = pn_current(self.snd)
    assert pn_delivery_tag(c) == "tag2", pn_delivery_tag(c)
    pn_settle(c)
    c = pn_current(self.snd)
    assert not c
    self.pump()

    c = pn_current(self.rcv)
    assert c
    assert pn_delivery_tag(c) == "tag", pn_delivery_tag(c)
    assert pn_remote_settled(c)
    pn_settle(c)
    c = pn_current(self.rcv)
    assert c
    assert pn_delivery_tag(c) == "tag2", pn_delivery_tag(c)
    assert pn_remote_settled(c)
    pn_settle(c)
    c = pn_current(self.rcv)
    assert not c

  def testUnsettled(self):
    pn_flow(self.rcv, 10)
    self.pump()

    assert pn_unsettled(self.snd) == 0, pn_unsettled(self.snd)
    assert pn_unsettled(self.rcv) == 0, pn_unsettled(self.rcv)

    d = pn_delivery(self.snd, "tag")
    assert d
    assert pn_unsettled(self.snd) == 1, pn_unsettled(self.snd)
    assert pn_unsettled(self.rcv) == 0, pn_unsettled(self.rcv)
    assert pn_advance(self.snd)
    self.pump()

    assert pn_unsettled(self.snd) == 1, pn_unsettled(self.snd)
    assert pn_unsettled(self.rcv) == 1, pn_unsettled(self.rcv)

    c = pn_current(self.rcv)
    assert c
    pn_settle(c)

    assert pn_unsettled(self.snd) == 1, pn_unsettled(self.snd)
    assert pn_unsettled(self.rcv) == 0, pn_unsettled(self.rcv)

class PipelineTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()

  def teardown(self):
    self.cleanup()

  def test(self):
    ssn = pn_session(self.c1)
    snd = pn_sender(ssn, "sender")
    pn_connection_open(self.c1)
    pn_session_open(ssn)
    pn_link_open(snd)

    for i in range(10):
      d = pn_delivery(snd, "delivery-%s" % i)
      pn_send(snd, "delivery-%s" % i)
      pn_settle(d)

    pn_link_close(snd)
    pn_session_close(ssn)
    pn_connection_close(self.c1)

    self.pump()

    assert pn_connection_state(self.c2) == (PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    ssn2 = pn_session_head(self.c2, PN_LOCAL_UNINIT)
    assert ssn2
    assert pn_session_state(ssn2) == (PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    rcv = pn_link_head(self.c2, PN_LOCAL_UNINIT)
    assert rcv
    assert pn_link_state(rcv) == (PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)

    pn_connection_open(self.c2)
    pn_session_open(ssn2)
    pn_link_open(rcv)
    pn_flow(rcv, 10)
    assert pn_queued(rcv) == 0

    self.pump()

    assert pn_queued(rcv) == 10
    assert pn_link_state(rcv) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)
    assert pn_session_state(ssn2) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)
    assert pn_connection_state(self.c2) == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED)

    for i in range(pn_queued(rcv)):
      d = pn_current(rcv)
      assert d
      assert pn_delivery_tag(d) == "delivery-%s" % i
      pn_settle(d)

    assert pn_queued(rcv) == 0
