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
from xproton import *

# future test areas
# different permutations of setup
#   - creating deliveries and calling input/output before opening the session/link

OUTPUT_SIZE = 1024

def pump(t1, t2):
  while True:
    cd, out1 = pn_output(t1, OUTPUT_SIZE)
    assert cd >= 0 or cd == PN_EOS, (cd, out1, len(out1))
    cd, out2 = pn_output(t2, OUTPUT_SIZE)
    assert cd >= 0 or cd == PN_EOS, (cd, out2, len(out2))

    if out1 or out2:
      if out1:
        cd = pn_input(t2, out1)
        assert cd == PN_EOS or cd == len(out1), (cd, out1, len(out1))
      if out2:
        cd = pn_input(t1, out2)
        assert cd == PN_EOS or cd == len(out2), (cd, out2, len(out2))
    else:
      return

class Test(common.Test):

  def setup(self):
    self.c1 = pn_connection()
    self.c2 = pn_connection()
    self.t1 = pn_transport(self.c1)
    self.t2 = pn_transport(self.c2)
    trc = os.environ.get("PN_TRACE_FRM")
    if trc and trc.lower() in ("1", "2", "yes", "true"):
      pn_trace(self.t1, PN_TRACE_FRM)
    if trc == "2":
      pn_trace(self.t2, PN_TRACE_FRM)

  def teardown(self):
    pn_connection_destroy(self.c1)
    pn_connection_destroy(self.c2)

  def pump(self):
    pump(self.t1, self.t2)

class ConnectionTest(Test):

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
    Test.setup(self)
    self.ssn = pn_session(self.c1)
    pn_connection_open(self.c1)
    pn_connection_open(self.c2)

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

class LinkTest(Test):

  def setup(self):
    Test.setup(self)
    pn_connection_open(self.c1)
    pn_connection_open(self.c2)
    self.ssn1 = pn_session(self.c1)
    pn_session_open(self.ssn1)
    self.pump()
    self.ssn2 = pn_session_head(self.c2, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    pn_session_open(self.ssn2)
    self.pump()
    self.snd = pn_sender(self.ssn1, "test-link")
    self.rcv = pn_receiver(self.ssn2, "test-link")

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

class TransferTest(Test):

  def setup(self):
    Test.setup(self)
    self.ssn1 = pn_session(self.c1)
    pn_connection_open(self.c1)
    pn_connection_open(self.c2)
    pn_session_open(self.ssn1)
    self.pump()
    self.ssn2 = pn_session_head(self.c2, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE)
    assert self.ssn2 != None
    pn_session_open(self.ssn2)

    self.snd = pn_sender(self.ssn1, "test-link")
    self.rcv = pn_receiver(self.ssn2, "test-link")
    pn_link_open(self.snd)
    pn_link_open(self.rcv)
    self.pump()

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
    assert pn_delivery_tag(d) == "tag"
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

    rdisp = pn_remote_disp(sd)
    ldisp = pn_local_disp(rd)
    assert rdisp == ldisp == PN_ACCEPTED, (rdisp, ldisp)
    assert pn_updated(sd)

    pn_disposition(sd, PN_ACCEPTED)
    pn_settle(sd)

    self.pump()

    assert pn_local_disp(sd) == pn_remote_disp(rd) == PN_ACCEPTED
