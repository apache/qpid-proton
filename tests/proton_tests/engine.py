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
    c1._transport = t1
    t2 = Transport()
    t2.bind(c2)
    c2._transport = t2
    self._wires.append((c1, t1, c2, t2))
    trc = os.environ.get("PN_TRACE_FRM")
    if trc and trc.lower() in ("1", "2", "yes", "true"):
      t1.trace(Transport.TRACE_FRM)
    if trc == "2":
      t2.trace(Transport.TRACE_FRM)
    return c1, c2

  def link(self, name, max_frame=None, idle_timeout=None):
    c1, c2 = self.connection()
    if max_frame:
      c1._transport.max_frame_size = max_frame[0]
      c2._transport.max_frame_size = max_frame[1]
    if idle_timeout:
      c1._transport.idle_timeout = idle_timeout[0]
      c2._transport.idle_timeout = idle_timeout[1]
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

  def test_capabilities(self):
    self.c1.offered_capabilities.put_array(False, Data.SYMBOL)
    self.c1.offered_capabilities.enter()
    self.c1.offered_capabilities.put_symbol("O_one")
    self.c1.offered_capabilities.put_symbol("O_two")
    self.c1.offered_capabilities.put_symbol("O_three")
    self.c1.offered_capabilities.exit()

    self.c1.desired_capabilities.put_array(False, Data.SYMBOL)
    self.c1.desired_capabilities.enter()
    self.c1.desired_capabilities.put_symbol("D_one")
    self.c1.desired_capabilities.put_symbol("D_two")
    self.c1.desired_capabilities.put_symbol("D_three")
    self.c1.desired_capabilities.exit()

    self.c1.open()

    assert self.c2.remote_offered_capabilities.format() == ""
    assert self.c2.remote_desired_capabilities.format() == ""

    self.pump()

    assert self.c2.remote_offered_capabilities.format() == self.c1.offered_capabilities.format()
    assert self.c2.remote_desired_capabilities.format() == self.c1.desired_capabilities.format()

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

  def assertEqualTermini(self, t1, t2):
    assert t1.type == t2.type, (t1.type, t2.type)
    assert t1.address == t2.address, (t1.address, t2.address)
    assert t1.durability == t2.durability, (t1.durability, t2.durability)
    assert t1.expiry_policy == t2.expiry_policy, (t1.expiry_policy, t2.expiry_policy)
    assert t1.timeout == t2.timeout, (t1.timeout, t2.timeout)
    assert t1.dynamic == t2.dynamic, (t1.dynamic, t2.dynamic)
    for attr in ["properties", "capabilities", "outcomes", "filter"]:
      d1 = getattr(t1, attr)
      d2 = getattr(t2, attr)
      assert d1.format() == d2.format(), (attr, d1.format(), d2.format())

  def _test_source_target(self, config_source, config_target):
    if config_source is None:
      self.snd.source.type = Terminus.UNSPECIFIED
    else:
      config_source(self.snd.source)
    if config_target is None:
      self.snd.target.type = Terminus.UNSPECIFIED
    else:
      config_target(self.snd.target)
    self.snd.open()
    self.pump()
    self.assertEqualTermini(self.rcv.remote_source, self.snd.source)
    self.assertEqualTermini(self.rcv.remote_target, self.snd.target)
    self.rcv.target.copy(self.rcv.remote_target)
    self.rcv.source.copy(self.rcv.remote_source)
    self.rcv.open()
    self.pump()
    self.assertEqualTermini(self.snd.remote_target, self.snd.target)
    self.assertEqualTermini(self.snd.remote_source, self.snd.source)

  def test_source_target(self):
    self._test_source_target(TerminusConfig(address="source"),
                             TerminusConfig(address="target"))

  def test_source(self):
    self._test_source_target(TerminusConfig(address="source"), None)

  def test_target(self):
    self._test_source_target(None, TerminusConfig(address="target"))

  def test_source_target_full(self):
    self._test_source_target(TerminusConfig(address="source",
                                            timeout=3,
                                            filter=[("int", 1), ("symbol", "two"), ("string", "three")],
                                            capabilities=["one", "two", "three"]),
                             TerminusConfig(address="source",
                                            timeout=7,
                                            capabilities=[]))

class TerminusConfig:

  def __init__(self, address=None, timeout=None, durability=None, filter=None,
               capabilities=None):
    self.address = address
    self.timeout = timeout
    self.durability = durability
    self.filter = filter
    self.capabilities = capabilities

  def __call__(self, terminus):
    if self.address is not None:
      terminus.address = self.address
    if self.timeout is not None:
      terminus.timeout = self.timeout
    if self.durability is not None:
      terminus.durability = self.durability
    if self.capabilities is not None:
      terminus.capabilities.put_array(False, Data.SYMBOL)
      terminus.capabilities.enter()
      for c in self.capabilities:
        terminus.capabilities.put_symbol(c)
    if self.filter is not None:
      terminus.filter.put_list()
      terminus.filter.enter()
      for (t, v) in self.filter:
        setter = getattr(terminus.filter, "put_%s" % t)
        setter(v)

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
    rd.update(Delivery.ACCEPTED)

    self.pump()

    rdisp = sd.remote_state
    ldisp = rd.local_state
    assert rdisp == ldisp == Delivery.ACCEPTED, (rdisp, ldisp)
    assert sd.updated

    sd.update(Delivery.ACCEPTED)
    sd.settle()

    self.pump()

    assert sd.local_state == rd.remote_state == Delivery.ACCEPTED

class MaxFrameTransferTest(Test):

  def setup(self):
    pass

  def teardown(self):
    self.cleanup()

  def message(self, size):
    parts = []
    for i in range(size):
      parts.append(str(i))
    return "/".join(parts)[:size]

  def testMinFrame(self):
    """
    Configure receiver to support minimum max-frame as defined by AMQP-1.0.
    Verify transfer of messages larger than 512.
    """
    self.snd, self.rcv = self.link("test-link", max_frame=[0,512])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()
    assert self.rcv.session.connection._transport.max_frame_size == 512
    assert self.snd.session.connection._transport.remote_max_frame_size == 512

    self.rcv.flow(1)
    self.snd.delivery("tag")
    msg = self.message(513)
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(513)
    assert bytes == msg

    bytes = self.rcv.recv(1024)
    assert bytes == None

  def testOddFrame(self):
    """
    Test an odd sized max limit with data that will require multiple frames to
    be transfered.
    """
    self.snd, self.rcv = self.link("test-link", max_frame=[0,521])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()
    assert self.rcv.session.connection._transport.max_frame_size == 521
    assert self.snd.session.connection._transport.remote_max_frame_size == 521

    self.rcv.flow(2)
    self.snd.delivery("tag")
    msg = "X" * 1699
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(1699)
    assert bytes == msg

    bytes = self.rcv.recv(1024)
    assert bytes == None

    self.rcv.advance()

    self.snd.delivery("gat")
    msg = self.message(1426)
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(1426)
    assert bytes == msg

    self.pump()

    bytes = self.rcv.recv(1024)
    assert bytes == None

class IdleTimeoutTest(Test):

  def setup(self):
    pass

  def teardown(self):
    self.cleanup()

  def message(self, size):
    parts = []
    for i in range(size):
      parts.append(str(i))
    return "/".join(parts)[:size]

  def testGetSet(self):
    """
    Verify the configuration and negotiation of the idle timeout.
    """

    self.snd, self.rcv = self.link("test-link", idle_timeout=[1000,2000])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()
    assert self.rcv.session.connection._transport.idle_timeout == 2000
    assert self.rcv.session.connection._transport.remote_idle_timeout == 1000
    assert self.snd.session.connection._transport.idle_timeout == 1000
    assert self.snd.session.connection._transport.remote_idle_timeout == 2000

  def testTimeout(self):
    """
    Verify the AMQP Connection idle timeout.
    """

    # snd will timeout the Connection if no frame is received within 1000 ticks
    self.snd, self.rcv = self.link("test-link", idle_timeout=[1000,0])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

    t_snd = self.snd.session.connection._transport
    t_rcv = self.rcv.session.connection._transport
    assert t_rcv.idle_timeout == 0
    assert t_rcv.remote_idle_timeout == 1000
    assert t_snd.idle_timeout == 1000
    assert t_snd.remote_idle_timeout == 0

    sndr_frames_in = t_snd.frames_input
    rcvr_frames_out = t_rcv.frames_output

    # at t+1, nothing should happen:
    clock = 1
    assert t_snd.tick(clock) == 1001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 501,  "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # at one tick from expected idle frame send, nothing should happen:
    clock = 500
    assert t_snd.tick(clock) == 1001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 501,  "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # this should cause rcvr to expire and send a keepalive
    clock = 502
    assert t_snd.tick(clock) == 1001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 1002, "deadline to send keepalive"
    self.pump()
    sndr_frames_in += 1
    rcvr_frames_out += 1
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"
    assert rcvr_frames_out == t_rcv.frames_output, "unexpected frame"

    # since a keepalive was received, sndr will rebase its clock against this tick:
    # and the receiver should not change its deadline
    clock = 503
    assert t_snd.tick(clock) == 1503, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 1002, "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # now expire sndr
    clock = 1504
    t_snd.tick(clock)
    try:
      self.pump()
      assert False, "Expected connection timeout did not happen!"
    except TransportException:
      pass

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
    assert c.settled
    c.settle()
    c = self.rcv.current
    assert c
    assert c.tag == "tag2", c.tag
    assert c.settled
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
