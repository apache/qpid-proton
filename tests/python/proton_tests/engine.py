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

import os, common, gc
from time import time, sleep
from proton import *
from common import pump

# older versions of gc do not provide the garbage list
if not hasattr(gc, "garbage"):
  gc.garbage=[]

# future test areas
#  + different permutations of setup
#   - creating deliveries and calling input/output before opening the session/link
#  + shrinking output_size down to something small? should the enginge buffer?
#  + resuming
#    - locally and remotely created deliveries with the same tag

OUTPUT_SIZE = 10*1024

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

    mask1 = 0
    mask2 = 0

    for cat in ("TRACE_FRM", "TRACE_RAW"):
      trc = os.environ.get("PN_%s" % cat)
      if trc and trc.lower() in ("1", "2", "yes", "true"):
        mask1 = mask1 | getattr(Transport, cat)
      if trc == "2":
        mask2 = mask2 | getattr(Transport, cat)
    t1.trace(mask1)
    t2.trace(mask2)

    return c1, c2

  def link(self, name, max_frame=None, idle_timeout=None):
    c1, c2 = self.connection()
    if max_frame:
      c1._transport.max_frame_size = max_frame[0]
      c2._transport.max_frame_size = max_frame[1]
    if idle_timeout:
      # idle_timeout in seconds expressed as float
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
    # release resources created by this class
    for w in self._wires:
        w[0]._transport = None
        w[2]._transport = None
    self._wires = []

  def pump(self, buffer_size=OUTPUT_SIZE):
    for c1, t1, c2, t2 in self._wires:
      pump(t1, t2, buffer_size)

class ConnectionTest(Test):

  def setup(self):
    gc.enable()
    self.c1, self.c2 = self.connection()

  def cleanup(self):
    # release resources created by this class
    super(ConnectionTest, self).cleanup()
    self.c1 = None
    self.c2 = None

  def teardown(self):
    self.cleanup()
    gc.collect()
    assert not gc.garbage

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
    self.c1.offered_capabilities = Array(UNDESCRIBED, Data.SYMBOL,
                                         symbol("O_one"),
                                         symbol("O_two"),
                                         symbol("O_three"))

    self.c1.desired_capabilities = Array(UNDESCRIBED, Data.SYMBOL,
                                         symbol("D_one"),
                                         symbol("D_two"),
                                         symbol("D_three"))
    self.c1.open()

    assert self.c2.remote_offered_capabilities is None
    assert self.c2.remote_desired_capabilities is None

    self.pump()

    assert self.c2.remote_offered_capabilities == self.c1.offered_capabilities, \
        (self.c2.remote_offered_capabilities, self.c1.offered_capabilities)
    assert self.c2.remote_desired_capabilities == self.c1.desired_capabilities, \
        (self.c2.remote_desired_capabilities, self.c1.desired_capabilities)

  def test_condition(self):
    self.c1.open()
    self.c2.open()
    self.pump()
    assert self.c1.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    cond = Condition("blah:bleh", "this is a description", {symbol("foo"): "bar"})
    self.c1.condition = cond
    self.c1.close()

    self.pump()

    assert self.c1.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    rcond = self.c2.remote_condition
    assert rcond == cond, (rcond, cond)

  def test_properties(self, p1={symbol("key"): symbol("value")}, p2=None):
    self.c1.properties = p1
    self.c2.properties = p2
    self.c1.open()
    self.c2.open()
    self.pump()

    assert self.c2.remote_properties == p1, (self.c2.remote_properties, p1)
    assert self.c1.remote_properties == p2, (self.c2.remote_properties, p2)

  def test_channel_max(self, value=1234):
    self.c1._transport.channel_max = value
    self.c1.open()
    self.pump()
    assert self.c2._transport.remote_channel_max == value, (self.c2._transport.remote_channel_max, value)

  def test_cleanup(self):
    self.c1.open()
    self.c2.open()
    self.pump()
    assert self.c1.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    t1 = self.c1._transport
    t2 = self.c2._transport
    c2 = self.c2
    self.c1.close()
    # release all references to C1, except that held by the transport
    self.cleanup()
    gc.collect()
    # transport should flush last state from C1:
    pump(t1, t2)
    assert c2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

class SessionTest(Test):

  def setup(self):
    gc.enable()
    self.c1, self.c2 = self.connection()
    self.ssn = self.c1.session()
    self.c1.open()
    self.c2.open()

  def cleanup(self):
    # release resources created by this class
    super(SessionTest, self).cleanup()
    self.c1 = None
    self.c2 = None
    self.ssn = None

  def teardown(self):
    self.cleanup()
    gc.collect()
    assert not gc.garbage

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

  def test_condition(self):
    self.ssn.open()
    self.pump()
    ssn = self.c2.session_head(Endpoint.REMOTE_ACTIVE | Endpoint.LOCAL_UNINIT)
    assert ssn != None
    ssn.open()
    self.pump()

    assert self.ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    cond = Condition("blah:bleh", "this is a description", {symbol("foo"): "bar"})
    self.ssn.condition = cond
    self.ssn.close()

    self.pump()

    assert self.ssn.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    rcond = ssn.remote_condition
    assert rcond == cond, (rcond, cond)

  def test_cleanup(self):
    snd, rcv = self.link("test-link")
    snd.open()
    rcv.open()
    self.pump()
    snd_ssn = snd.session
    rcv_ssn = rcv.session
    assert rcv_ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    self.ssn = None
    snd_ssn.close()
    snd_ssn.free()
    del snd_ssn
    gc.collect()
    self.pump()
    assert rcv_ssn.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

class LinkTest(Test):

  def setup(self):
    gc.enable()
    self.snd, self.rcv = self.link("test-link")

  def cleanup(self):
    # release resources created by this class
    super(LinkTest, self).cleanup()
    self.snd = None
    self.rcv = None

  def teardown(self):
    self.cleanup()
    gc.collect()
    assert not gc.garbage

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
    assert rcv.name == "second-rcv"
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
                                            dist_mode=Terminus.DIST_MODE_MOVE,
                                            filter=[("int", 1), ("symbol", "two"), ("string", "three")],
                                            capabilities=["one", "two", "three"]),
                             TerminusConfig(address="source",
                                            timeout=7,
                                            capabilities=[]))
  def test_distribution_mode(self):
    self._test_source_target(TerminusConfig(address="source",
                                            dist_mode=Terminus.DIST_MODE_COPY),
                             TerminusConfig(address="target"))
    assert self.rcv.remote_source.distribution_mode == Terminus.DIST_MODE_COPY
    assert self.rcv.remote_target.distribution_mode == Terminus.DIST_MODE_UNSPECIFIED

  def test_dynamic_link(self):
    self._test_source_target(TerminusConfig(address=None, dynamic=True), None)
    assert self.rcv.remote_source.dynamic
    assert self.rcv.remote_source.address is None

  def test_condition(self):
    self.snd.open()
    self.rcv.open()
    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    cond = Condition("blah:bleh", "this is a description", {symbol("foo"): "bar"})
    self.snd.condition = cond
    self.snd.close()

    self.pump()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

    rcond = self.rcv.remote_condition
    assert rcond == cond, (rcond, cond)

  def test_settle_mode(self):
    self.snd.snd_settle_mode = Link.SND_UNSETTLED
    assert self.snd.snd_settle_mode == Link.SND_UNSETTLED
    self.rcv.rcv_settle_mode = Link.RCV_SECOND
    assert self.rcv.rcv_settle_mode == Link.RCV_SECOND

    assert self.snd.remote_rcv_settle_mode != Link.RCV_SECOND
    assert self.rcv.remote_snd_settle_mode != Link.SND_UNSETTLED

    self.snd.open()
    self.rcv.open()
    self.pump()

    assert self.snd.remote_rcv_settle_mode == Link.RCV_SECOND
    assert self.rcv.remote_snd_settle_mode == Link.SND_UNSETTLED

  def test_cleanup(self):
    snd, rcv = self.link("test-link")
    snd.open()
    rcv.open()
    self.pump()
    assert rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    snd.close()
    snd.free()
    del snd
    gc.collect()
    self.pump()
    assert rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_CLOSED

class TerminusConfig:

  def __init__(self, address=None, timeout=None, durability=None, filter=None,
               capabilities=None, dynamic=False, dist_mode=None):
    self.address = address
    self.timeout = timeout
    self.durability = durability
    self.filter = filter
    self.capabilities = capabilities
    self.dynamic = dynamic
    self.dist_mode = dist_mode

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
      terminus.filter.put_map()
      terminus.filter.enter()
      for (t, v) in self.filter:
        setter = getattr(terminus.filter, "put_%s" % t)
        setter(v)
    if self.dynamic:
      terminus.dynamic = True
    if self.dist_mode is not None:
      terminus.distribution_mode = self.dist_mode

class TransferTest(Test):

  def setup(self):
    gc.enable()
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def cleanup(self):
    # release resources created by this class
    super(TransferTest, self).cleanup()
    self.c1 = None
    self.c2 = None
    self.snd = None
    self.rcv = None

  def teardown(self):
    self.cleanup()
    gc.collect()
    assert not gc.garbage

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
    assert bytes == msg, (bytes, msg)

    bytes = self.rcv.recv(1024)
    assert bytes == ""

    msg = "this is more"
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(1024)
    assert bytes == msg, (bytes, msg)

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

    self.pump()

    assert sd.local_state == rd.remote_state == Delivery.ACCEPTED
    sd.settle()

  def test_delivery_id_ordering(self):
    self.rcv.flow(1024)
    self.pump(buffer_size=64*1024)

    #fill up delivery buffer on sender
    for m in range(1024):
      sd = self.snd.delivery("tag%s" % m)
      msg = "message %s" % m
      n = self.snd.send(msg)
      assert n == len(msg)
      assert self.snd.advance()

    self.pump(buffer_size=64*1024)

    #receive a session-windows worth of messages and accept them
    for m in range(1024):
      rd = self.rcv.current
      assert rd is not None, m
      assert rd.tag == ("tag%s" % m), (rd.tag, m)
      msg = self.rcv.recv(1024)
      assert msg == ("message %s" % m), (msg, m)
      rd.update(Delivery.ACCEPTED)
      rd.settle()

    self.pump(buffer_size=64*1024)

    #add some new deliveries
    for m in range(1024, 1450):
      sd = self.snd.delivery("tag%s" % m)
      msg = "message %s" % m
      n = self.snd.send(msg)
      assert n == len(msg)
      assert self.snd.advance()

    #handle all disposition changes to sent messages
    d = self.c1.work_head
    while d:
      next_d = d.work_next
      if d.updated:
        d.update(Delivery.ACCEPTED)
        d.settle()
      d = next_d

    #submit some more deliveries
    for m in range(1450, 1500):
      sd = self.snd.delivery("tag%s" % m)
      msg = "message %s" % m
      n = self.snd.send(msg)
      assert n == len(msg)
      assert self.snd.advance()

    self.pump(buffer_size=64*1024)
    self.rcv.flow(1024)
    self.pump(buffer_size=64*1024)

    #verify remaining messages can be received and accepted
    for m in range(1024, 1500):
      rd = self.rcv.current
      assert rd is not None, m
      assert rd.tag == ("tag%s" % m), (rd.tag, m)
      msg = self.rcv.recv(1024)
      assert msg == ("message %s" % m), (msg, m)
      rd.update(Delivery.ACCEPTED)
      rd.settle()

  def test_cleanup(self):
    self.rcv.flow(10)
    self.pump()

    for x in range(10):
        self.snd.delivery("tag%d" % x)
        msg = "this is a test"
        n = self.snd.send(msg)
        assert n == len(msg)
        assert self.snd.advance()
    self.snd.close()
    self.snd.free()
    self.snd = None
    gc.collect()

    self.pump()

    for x in range(10):
        rd = self.rcv.current
        assert rd is not None
        assert rd.tag == "tag%d" % x
        rmsg = self.rcv.recv(1024)
        assert self.rcv.advance()
        assert rmsg == msg
        # close of snd should've settled:
        assert rd.settled
        rd.settle()

class MaxFrameTransferTest(Test):

  def setup(self):
    pass

  def cleanup(self):
    # release resources created by this class
    super(MaxFrameTransferTest, self).cleanup()
    self.c1 = None
    self.c2 = None
    self.snd = None
    self.rcv = None

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
    
  def testBigMessage(self):
    """
    Test transfering a big message.
    """
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

    self.rcv.flow(2)
    self.snd.delivery("tag")
    msg = self.message(1024*256)
    n = self.snd.send(msg)
    assert n == len(msg)
    assert self.snd.advance()

    self.pump()

    bytes = self.rcv.recv(1024*256)
    assert bytes == msg

    bytes = self.rcv.recv(1024)
    assert bytes == None


class IdleTimeoutTest(Test):

  def setup(self):
    pass

  def cleanup(self):
    # release resources created by this class
    super(IdleTimeoutTest, self).cleanup()
    self.snd = None
    self.rcv = None
    self.c1 = None
    self.c2 = None

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

    self.snd, self.rcv = self.link("test-link", idle_timeout=[1.0,2.0])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()
    # proton advertises 1/2 the configured timeout to the peer:
    assert self.rcv.session.connection._transport.idle_timeout == 2.0
    assert self.rcv.session.connection._transport.remote_idle_timeout == 0.5
    assert self.snd.session.connection._transport.idle_timeout == 1.0
    assert self.snd.session.connection._transport.remote_idle_timeout == 1.0

  def testTimeout(self):
    """
    Verify the AMQP Connection idle timeout.
    """

    # snd will timeout the Connection if no frame is received within 1000 ticks
    self.snd, self.rcv = self.link("test-link", idle_timeout=[1.0,0])
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

    t_snd = self.snd.session.connection._transport
    t_rcv = self.rcv.session.connection._transport
    assert t_rcv.idle_timeout == 0.0
    # proton advertises 1/2 the timeout (see spec)
    assert t_rcv.remote_idle_timeout == 0.5
    assert t_snd.idle_timeout == 1.0
    assert t_snd.remote_idle_timeout == 0.0

    sndr_frames_in = t_snd.frames_input
    rcvr_frames_out = t_rcv.frames_output

    # at t+1msec, nothing should happen:
    clock = 0.001
    assert t_snd.tick(clock) == 1.001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 0.251,  "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # at one tick from expected idle frame send, nothing should happen:
    clock = 0.250
    assert t_snd.tick(clock) == 1.001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 0.251,  "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # this should cause rcvr to expire and send a keepalive
    clock = 0.251
    assert t_snd.tick(clock) == 1.001, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 0.501, "deadline to send keepalive"
    self.pump()
    sndr_frames_in += 1
    rcvr_frames_out += 1
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"
    assert rcvr_frames_out == t_rcv.frames_output, "unexpected frame"

    # since a keepalive was received, sndr will rebase its clock against this tick:
    # and the receiver should not change its deadline
    clock = 0.498
    assert t_snd.tick(clock) == 1.498, "deadline for remote timeout"
    assert t_rcv.tick(clock) == 0.501, "deadline to send keepalive"
    self.pump()
    assert sndr_frames_in == t_snd.frames_input, "unexpected received frame"

    # now expire sndr
    clock = 1.499
    t_snd.tick(clock)
    try:
      self.pump()
      assert False, "Expected connection timeout did not happen!"
    except TransportException:
      pass

class CreditTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link", max_frame=(16*1024, 16*1024))
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def cleanup(self):
    # release resources created by this class
    super(CreditTest, self).cleanup()
    self.c1 = None
    self.snd = None
    self.c2 = None
    self.rcv2 = None
    self.snd2 = None

  def teardown(self):
    self.cleanup()

  def testCreditSender(self, count=1024):
    credit = self.snd.credit
    assert credit == 0, credit
    self.rcv.flow(10)
    self.pump()
    credit = self.snd.credit
    assert credit == 10, credit

    self.rcv.flow(count)
    self.pump()
    credit = self.snd.credit
    assert credit == 10 + count, credit

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

  def testFullDrain(self):
    assert self.rcv.credit == 0
    assert self.snd.credit == 0
    self.rcv.drain(10)
    assert self.rcv.draining()
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    self.pump()
    assert self.rcv.credit == 10
    assert self.snd.credit == 10
    assert self.rcv.draining()
    self.snd.drained()
    assert self.rcv.credit == 10
    assert self.snd.credit == 0
    assert self.rcv.draining()
    self.pump()
    assert self.rcv.credit == 0
    assert self.snd.credit == 0
    assert not self.rcv.draining()
    drained = self.rcv.drained()
    assert drained == 10, drained

  def testPartialDrain(self):
    self.rcv.drain(2)
    assert self.rcv.draining()
    self.pump()

    d = self.snd.delivery("tag")
    assert d
    assert self.snd.advance()
    self.snd.drained()
    assert self.rcv.draining()
    self.pump()
    assert not self.rcv.draining()

    c = self.rcv.current
    assert self.rcv.queued == 1, self.rcv.queued
    assert c.tag == d.tag, c.tag
    assert self.rcv.advance()
    assert not self.rcv.current
    assert self.rcv.credit == 0, self.rcv.credit
    assert not self.rcv.draining()
    drained = self.rcv.drained()
    assert drained == 1, drained

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
    drained = self.rcv.drained()
    assert drained == 10, drained

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
    assert self.rcv.queued == 1, self.rcv.queued

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
    drained = self.rcv.drained()
    assert drained == 0

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
    drained = self.rcv.drained()
    assert drained == 0

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
    drained = self.rcv.drained()
    assert drained == 0
    self.pump()

    assert self.snd.credit == 0
    assert self.rcv.credit == 0
    assert self.rcv.queued == 0
    drained = self.rcv.drained()
    assert drained == 10


  def testDrainOrder(self):
    """ Verify drain/drained works regardless of ordering.  See PROTON-401
    """
    assert self.snd.credit == 0
    assert self.rcv.credit == 0
    assert self.rcv.queued == 0

    #self.rcv.session.connection._transport.trace(Transport.TRACE_FRM)
    #self.snd.session.connection._transport.trace(Transport.TRACE_FRM)

    ## verify that a sender that has reached the drain state will respond
    ## promptly to a drain issued by the peer.
    self.rcv.flow(10)
    self.pump()
    assert self.snd.credit == 10, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    sd = self.snd.delivery("tagA")
    assert sd
    n = self.snd.send("A")
    assert n == 1
    self.pump()
    self.snd.advance()

    # done sending, so signal that we are drained:
    self.snd.drained()
    self.pump()
    assert self.snd.credit == 9, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    self.rcv.drain(0)
    self.pump()
    assert self.snd.credit == 9, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    bytes = self.rcv.recv(10)
    assert bytes == "A", bytes
    self.rcv.advance()
    self.pump()
    assert self.snd.credit == 9, self.snd.credit
    assert self.rcv.credit == 9, self.rcv.credit

    self.snd.drained()
    self.pump()
    assert self.snd.credit == 0, self.snd.credit
    assert self.rcv.credit == 0, self.rcv.credit

    # verify that a drain requested by the peer is not "acknowledged" until
    # after the sender has completed sending its pending messages

    self.rcv.flow(10)
    self.pump()
    assert self.snd.credit == 10, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    sd = self.snd.delivery("tagB")
    assert sd
    n = self.snd.send("B")
    assert n == 1
    self.snd.advance()
    self.pump()
    assert self.snd.credit == 9, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    self.rcv.drain(0)
    self.pump()
    assert self.snd.credit == 9, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    sd = self.snd.delivery("tagC")
    assert sd
    n = self.snd.send("C")
    assert n == 1
    self.snd.advance()
    self.pump()
    assert self.snd.credit == 8, self.snd.credit
    assert self.rcv.credit == 10, self.rcv.credit

    # now that the sender has finished sending everything, it can signal
    # drained
    self.snd.drained()
    self.pump()
    assert self.snd.credit == 0, self.snd.credit
    assert self.rcv.credit == 2, self.rcv.credit

    bytes = self.rcv.recv(10)
    assert bytes == "B", bytes
    self.rcv.advance()
    bytes = self.rcv.recv(10)
    assert bytes == "C", bytes
    self.rcv.advance()
    self.pump()
    assert self.snd.credit == 0, self.snd.credit
    assert self.rcv.credit == 0, self.rcv.credit


  def testPushback(self, count=10):
    assert self.snd.credit == 0
    assert self.rcv.credit == 0

    self.rcv.flow(count)
    self.pump()

    for i in range(count):
      d = self.snd.delivery("tag%s" % i)
      assert d
      self.snd.advance()

    assert self.snd.queued == count
    assert self.rcv.queued == 0
    self.pump()
    assert self.snd.queued == 0
    assert self.rcv.queued == count

    d = self.snd.delivery("extra")
    self.snd.advance()

    assert self.snd.queued == 1
    assert self.rcv.queued == count
    self.pump()
    assert self.snd.queued == 1
    assert self.rcv.queued == count

  def testHeadOfLineBlocking(self):
      self.snd2 = self.snd.session.sender("link-2")
      self.rcv2 = self.rcv.session.receiver("link-2")
      self.snd2.open()
      self.rcv2.open()
      self.pump()
      assert self.snd2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
      assert self.rcv2.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

      self.rcv.flow(5)
      self.rcv2.flow(10)
      self.pump()

      assert self.snd.credit == 5
      assert self.snd2.credit == 10

      for i in range(10):
          tag = "test %d" % i
          self.snd.delivery( tag )
          self.snd.send( tag )
          assert self.snd.advance()
          self.snd2.delivery( tag )
          self.snd2.send( tag )
          assert self.snd2.advance()

      self.pump()

      for i in range(5):
          b = self.rcv.recv( 512 )
          assert self.rcv.advance()
          b = self.rcv2.recv( 512 )
          assert self.rcv2.advance()

      for i in range(5):
          b = self.rcv2.recv( 512 )
          assert self.rcv2.advance()



class SessionCreditTest(Test):

  def teardown(self):
    self.cleanup()

  def testBuffering(self, count=32, size=1024, capacity=16*1024, max_frame=1024):
    snd, rcv = self.link("test-link", max_frame=(max_frame, max_frame))
    rcv.session.incoming_capacity = capacity
    snd.open()
    rcv.open()
    rcv.flow(count)
    self.pump()

    assert count > 0

    total_bytes = count * size

    assert snd.session.outgoing_bytes == 0, snd.session.outgoing_bytes
    assert rcv.session.incoming_bytes == 0, rcv.session.incoming_bytes
    assert snd.queued == 0, snd.queued
    assert rcv.queued == 0, rcv.queued

    idx = 0
    while snd.credit:
      d = snd.delivery("tag%s" % idx)
      assert d
      n = snd.send(chr(ord("a") + idx)*size)
      assert n == size, (n, size)
      assert snd.advance()
      self.pump()
      idx += 1

    assert idx == count, (idx, count)

    assert snd.session.outgoing_bytes < total_bytes, (snd.session.outgoing_bytes, total_bytes)
    assert rcv.session.incoming_bytes < capacity, (rcv.session.incoming_bytes, capacity)
    assert snd.session.outgoing_bytes + rcv.session.incoming_bytes == total_bytes, \
        (snd.session.outgoing_bytes, rcv.session.incoming_bytes, total_bytes)
    if snd.session.outgoing_bytes > 0:
      available = rcv.session.incoming_capacity - rcv.session.incoming_bytes
      assert available < max_frame, (available, max_frame)

    for i in range(count):
      d = rcv.current
      assert d, i
      pending = d.pending
      before = rcv.session.incoming_bytes
      assert rcv.advance()
      after = rcv.session.incoming_bytes
      assert before - after == pending, (before, after, pending)
      snd_before = snd.session.incoming_bytes
      self.pump()
      snd_after = snd.session.incoming_bytes

      assert rcv.session.incoming_bytes < capacity
      if snd_before > 0:
        assert capacity - after <= max_frame
        assert snd_before > snd_after
      if snd_after > 0:
        available = rcv.session.incoming_capacity - rcv.session.incoming_bytes
        assert available < max_frame, available

  def testBufferingSize16(self):
    self.testBuffering(size=16)

  def testBufferingSize256(self):
    self.testBuffering(size=256)

  def testBufferingSize512(self):
    self.testBuffering(size=512)

  def testBufferingSize2048(self):
    self.testBuffering(size=2048)

  def testBufferingSize1025(self):
    self.testBuffering(size=1025)

  def testBufferingSize1023(self):
    self.testBuffering(size=1023)

  def testBufferingSize989(self):
    self.testBuffering(size=989)

  def testBufferingSize1059(self):
    self.testBuffering(size=1059)

  def testCreditWithBuffering(self):
    snd, rcv = self.link("test-link", max_frame=(1024, 1024))
    rcv.session.incoming_capacity = 64*1024
    snd.open()
    rcv.open()
    rcv.flow(128)
    self.pump()

    assert snd.credit == 128, snd.credit
    assert rcv.queued == 0, rcv.queued

    idx = 0
    while snd.credit:
      d = snd.delivery("tag%s" % idx)
      snd.send("x"*1024)
      assert d
      assert snd.advance()
      self.pump()
      idx += 1

    assert idx == 128, idx
    assert rcv.queued < 128, rcv.queued

    rcv.flow(1)
    self.pump()
    assert snd.credit == 1, snd.credit

class SettlementTest(Test):

  def setup(self):
    self.snd, self.rcv = self.link("test-link")
    self.c1 = self.snd.session.connection
    self.c2 = self.rcv.session.connection
    self.snd.open()
    self.rcv.open()
    self.pump()

  def cleanup(self):
    # release resources created by this class
    super(SettlementTest, self).cleanup()
    self.c1 = None
    self.snd = None
    self.c2 = None
    self.rcv2 = None
    self.snd2 = None

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

  def testMultipleUnsettled(self, count=1024, size=1024):
    self.rcv.flow(count)
    self.pump()

    assert self.snd.unsettled == 0, self.snd.unsettled
    assert self.rcv.unsettled == 0, self.rcv.unsettled

    unsettled = []

    for i in range(count):
      sd = self.snd.delivery("tag%s" % i)
      assert sd
      n = self.snd.send("x"*size)
      assert n == size, n
      assert self.snd.advance()
      self.pump()

      rd = self.rcv.current
      assert rd, "did not receive delivery %s" % i
      n = rd.pending
      b = self.rcv.recv(n)
      assert len(b) == n, (b, n)
      rd.update(Delivery.ACCEPTED)
      assert self.rcv.advance()
      self.pump()
      unsettled.append(rd)

    assert self.rcv.unsettled == count

    for rd in unsettled:
      rd.settle()

  def testMultipleUnsettled2K1K(self):
    self.testMultipleUnsettled(2048, 1024)

  def testMultipleUnsettled4K1K(self):
    self.testMultipleUnsettled(4096, 1024)

  def testMultipleUnsettled1K2K(self):
    self.testMultipleUnsettled(1024, 2048)

  def testMultipleUnsettled2K2K(self):
    self.testMultipleUnsettled(2048, 2048)

  def testMultipleUnsettled4K2K(self):
    self.testMultipleUnsettled(4096, 2048)

class PipelineTest(Test):

  def setup(self):
    self.c1, self.c2 = self.connection()

  def cleanup(self):
    # release resources created by this class
    super(PipelineTest, self).cleanup()
    self.c1 = None
    self.c2 = None

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

import sys
from common import Skipped

class ServerTest(Test):

  def testKeepalive(self):
    """ Verify that idle frames are sent to keep a Connection alive
    """
    if "java" in sys.platform:
      raise Skipped()
    idle_timeout_secs = self.delay
    self.server = common.TestServerDrain()
    self.server.start()
    self.driver = Driver()
    self.cxtr = self.driver.connector(self.server.host, self.server.port)
    self.cxtr.transport.idle_timeout = idle_timeout_secs
    self.cxtr.sasl().mechanisms("ANONYMOUS")
    self.cxtr.sasl().client()
    self.conn = Connection()
    self.cxtr.connection = self.conn
    self.conn.open()
    #self.session = self.conn.session()
    #self.session.open()
    #self.link = self.session.sender("test-sender")
    #self.link.open()

    # wait for the connection to come up

    deadline = time() + self.timeout
    while self.conn.state != (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE) \
          and time() <= deadline:
      self.cxtr.process()
      self.driver.wait(0.001)
      self.cxtr.process()

    assert self.conn.state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE), "Connection failed"

    # wait up to 3x the idle timeout
    old_count = self.cxtr.transport.frames_input
    duration = 3 * idle_timeout_secs
    deadline = time() + duration
    while time() <= deadline:
      self.cxtr.process()
      self.driver.wait(0.001)
      self.cxtr.process()

    assert self.conn.state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE), "Connection terminated"
    assert self.cxtr.transport.frames_input > old_count, "No idle frames received"

    self.server.stop()

  def testIdleTimeout(self):
    """ Verify that a Connection is terminated properly when Idle frames do not
    arrive in a timely manner.
    """
    if "java" in sys.platform:
      raise Skipped()
    idle_timeout_secs = self.delay
    self.server = common.TestServerDrain(idle_timeout=idle_timeout_secs)
    self.server.start()
    self.driver = Driver()
    self.cxtr = self.driver.connector(self.server.host, self.server.port)
    self.cxtr.sasl().mechanisms("ANONYMOUS")
    self.cxtr.sasl().client()
    self.conn = Connection()
    self.cxtr.connection = self.conn
    self.conn.open()

    # wait for the connection to come up

    deadline = time() + self.timeout
    while self.conn.state != (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE) \
          and time() <= deadline:
      self.cxtr.process()
      self.driver.wait(self.timeout)
      self.cxtr.process()

    assert self.conn.state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE), "Connection failed"

    # verify the connection stays up even if we don't explicitly send stuff
    # wait up to 3x the idle timeout
    old_count = self.cxtr.transport.frames_output
    duration = 3 * idle_timeout_secs
    deadline = time() + duration
    while time() <= deadline:
      self.cxtr.process()
      self.driver.wait(10 * duration)
      self.cxtr.process()

    assert self.conn.state == (Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE), "Connection terminated"
    assert self.cxtr.transport.frames_output > old_count, "No idle frames sent"

    # now wait to explicitly cause the other side to expire:

    sleep(idle_timeout_secs * 3)

    # and check that the remote killed the connection:

    deadline = time() + self.timeout
    while (self.conn.state & Endpoint.REMOTE_ACTIVE) and time() <= deadline:
      self.cxtr.process()
      self.driver.wait(self.timeout)
      self.cxtr.process()

    assert self.conn.state & Endpoint.REMOTE_CLOSED, "Connection failed to close"

    self.server.stop()

class NoValue:

  def __init__(self):
    pass

  def apply(self, dlv):
    pass

  def check(self, dlv):
    assert dlv.data == None
    assert dlv.section_number == 0
    assert dlv.section_offset == 0
    assert dlv.condition == None
    assert dlv.failed == False
    assert dlv.undeliverable == False
    assert dlv.annotations == None

class RejectValue:
  def __init__(self, condition):
    self.condition = condition

  def apply(self, dlv):
    dlv.condition = self.condition

  def check(self, dlv):
    assert dlv.data == None, dlv.data
    assert dlv.section_number == 0
    assert dlv.section_offset == 0
    assert dlv.condition == self.condition, (dlv.condition, self.condition)
    assert dlv.failed == False
    assert dlv.undeliverable == False
    assert dlv.annotations == None

class ReceivedValue:
  def __init__(self, section_number, section_offset):
    self.section_number = section_number
    self.section_offset = section_offset

  def apply(self, dlv):
    dlv.section_number = self.section_number
    dlv.section_offset = self.section_offset

  def check(self, dlv):
    assert dlv.data == None, dlv.data
    assert dlv.section_number == self.section_number, (dlv.section_number, self.section_number)
    assert dlv.section_offset == self.section_offset
    assert dlv.condition == None
    assert dlv.failed == False
    assert dlv.undeliverable == False
    assert dlv.annotations == None

class ModifiedValue:
  def __init__(self, failed, undeliverable, annotations):
    self.failed = failed
    self.undeliverable = undeliverable
    self.annotations = annotations

  def apply(self, dlv):
    dlv.failed = self.failed
    dlv.undeliverable = self.undeliverable
    dlv.annotations = self.annotations

  def check(self, dlv):
    assert dlv.data == None, dlv.data
    assert dlv.section_number == 0
    assert dlv.section_offset == 0
    assert dlv.condition == None
    assert dlv.failed == self.failed
    assert dlv.undeliverable == self.undeliverable
    assert dlv.annotations == self.annotations, (dlv.annotations, self.annotations)

class CustomValue:
  def __init__(self, data):
    self.data = data

  def apply(self, dlv):
    dlv.data = self.data

  def check(self, dlv):
    assert dlv.data == self.data, (dlv.data, self.data)
    assert dlv.section_number == 0
    assert dlv.section_offset == 0
    assert dlv.condition == None
    assert dlv.failed == False
    assert dlv.undeliverable == False
    assert dlv.annotations == None

class DeliveryTest(Test):

  def teardown(self):
    self.cleanup()

  def testDisposition(self, count=1, tag="tag%i", type=Delivery.ACCEPTED, value=NoValue()):
    snd, rcv = self.link("test-link")
    snd.open()
    rcv.open()

    snd_deliveries = []
    for i in range(count):
      d = snd.delivery(tag % i)
      snd_deliveries.append(d)
      snd.advance()

    rcv.flow(count)
    self.pump()

    rcv_deliveries = []
    for i in range(count):
      d = rcv.current
      assert d.tag == (tag % i)
      rcv_deliveries.append(d)
      rcv.advance()

    for d in rcv_deliveries:
      value.apply(d.local)
      d.update(type)

    self.pump()

    for d in snd_deliveries:
      assert d.remote_state == type
      assert d.remote.type == type
      value.check(d.remote)
      value.apply(d.local)
      d.update(type)

    self.pump()

    for d in rcv_deliveries:
      assert d.remote_state == type
      assert d.remote.type == type
      value.check(d.remote)

    for d in snd_deliveries:
      d.settle()

    self.pump()

    for d in rcv_deliveries:
      assert d.settled, d.settled
      d.settle()

  def testReceived(self):
    self.testDisposition(type=Disposition.RECEIVED, value=ReceivedValue(1, 2))

  def testRejected(self):
    self.testDisposition(type=Disposition.REJECTED, value=RejectValue(Condition(symbol("foo"))))

  def testReleased(self):
    self.testDisposition(type=Disposition.RELEASED)

  def testModified(self):
    self.testDisposition(type=Disposition.MODIFIED,
                         value=ModifiedValue(failed=True, undeliverable=True,
                                             annotations={"key": "value"}))

  def testCustom(self):
    self.testDisposition(type=0x12345, value=CustomValue([1, 2, 3]))

class EventTest(Test):

  def teardown(self):
    self.cleanup()

  def list(self, collector):
    result = []
    while True:
      e = collector.peek()
      if e:
        result.append(e)
        collector.pop()
      else:
        break
    return result

  def expect(self, collector, *types):
    events = self.list(collector)
    assert types == tuple([e.type for e in events]), (types, events)
    if len(events) == 1:
      return events[0]
    elif len(events) > 1:
      return events

  def testEndpointEvents(self):
    c1, c2 = self.connection()
    coll = Collector()
    c1.collect(coll)
    self.expect(coll)
    self.pump()
    self.expect(coll)
    c2.open()
    self.pump()
    self.expect(coll, Event.CONNECTION_STATE)
    self.pump()
    self.expect(coll)

    ssn = c2.session()
    snd = ssn.sender("sender")
    ssn.open()
    snd.open()

    self.expect(coll)
    self.pump()
    self.expect(coll, Event.SESSION_STATE, Event.LINK_STATE)

  def testFlowEvents(self):
    snd, rcv = self.link("test-link")
    coll = Collector()
    snd.session.connection.collect(coll)
    rcv.open()
    rcv.flow(10)
    self.pump()
    self.expect(coll, Event.LINK_STATE, Event.LINK_FLOW)
    rcv.flow(10)
    self.pump()
    self.expect(coll, Event.LINK_FLOW)
    return snd, rcv, coll

  def testDeliveryEvents(self):
    snd, rcv = self.link("test-link")
    coll = Collector()
    rcv.session.connection.collect(coll)
    rcv.open()
    rcv.flow(10)
    self.pump()
    self.expect(coll, Event.TRANSPORT, Event.TRANSPORT)
    snd.delivery("delivery")
    snd.send("Hello World!")
    snd.advance()
    self.pump()
    self.expect(coll)
    snd.open()
    self.pump()
    self.expect(coll, Event.LINK_STATE, Event.DELIVERY)

  def testDeliveryEventsDisp(self):
    snd, rcv, coll = self.testFlowEvents()
    snd.open()
    dlv = snd.delivery("delivery")
    snd.send("Hello World!")
    assert snd.advance()
    self.expect(coll, Event.TRANSPORT, Event.TRANSPORT, Event.TRANSPORT)
    self.pump()
    self.expect(coll)
    rdlv = rcv.current
    assert rdlv != None
    assert rdlv.tag == "delivery"
    rdlv.update(Delivery.ACCEPTED)
    self.pump()
    event = self.expect(coll, Event.DELIVERY)
    assert event.delivery == dlv
