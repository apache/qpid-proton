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
from org.apache.qpid.proton import Proton
from org.apache.qpid.proton.amqp import Symbol
from org.apache.qpid.proton.amqp.messaging import Source, Target, \
  TerminusDurability, TerminusExpiryPolicy, Received, Accepted, \
  Rejected, Released, Modified
from org.apache.qpid.proton.amqp.transaction import Coordinator
from org.apache.qpid.proton.amqp.transport import ErrorCondition, \
  SenderSettleMode, ReceiverSettleMode
from org.apache.qpid.proton.engine import EndpointState, Sender, \
  Receiver, TransportException

from java.util import EnumSet
from jarray import array, zeros

from cerror import *
from ccodec import *

# from proton/engine.h
PN_LOCAL_UNINIT = 1
PN_LOCAL_ACTIVE = 2
PN_LOCAL_CLOSED = 4
PN_REMOTE_UNINIT = 8
PN_REMOTE_ACTIVE = 16
PN_REMOTE_CLOSED = 32

PN_SND_UNSETTLED = 0
PN_SND_SETTLED = 1
PN_SND_MIXED = 2

PN_RCV_FIRST = 0
PN_RCV_SECOND = 1

PN_UNSPECIFIED = 0
PN_SOURCE = 1
PN_TARGET = 2
PN_COORDINATOR = 3

PN_NONDURABLE = 0
PN_CONFIGURATION = 1
PN_DELIVERIES = 2

PN_LINK_CLOSE = 0
PN_SESSION_CLOSE = 1
PN_CONNECTION_CLOSE = 2
PN_NEVER = 3

PN_DIST_MODE_UNSPECIFIED = 0
PN_DIST_MODE_COPY = 1
PN_DIST_MODE_MOVE = 2

PN_RECEIVED = (0x0000000000000023)
PN_ACCEPTED = (0x0000000000000024)
PN_REJECTED = (0x0000000000000025)
PN_RELEASED = (0x0000000000000026)
PN_MODIFIED = (0x0000000000000027)

PN_TRACE_OFF = (0)
PN_TRACE_RAW = (1)
PN_TRACE_FRM = (2)
PN_TRACE_DRV = (4)

def wrap(obj, wrapper):
  if obj:
    ctx = obj.getContext()
    if not ctx:
      ctx = wrapper(obj)
      obj.setContext(ctx)
    return ctx

class pn_condition:

  def __init__(self):
    self.name = None
    self.description = None
    self.info = pn_data(0)

  def decode(self, impl):
    if impl is None:
      self.name = None
      self.description = None
      self.info.clear()
    else:
      self.name = impl.getCondition().toString()
      self.description = impl.getDescription()
      obj2dat(impl.getInfo(), self.info)

  def encode(self):
    if self.name is None:
      return None
    else:
      impl = ErrorCondition()
      impl.setCondition(Symbol.valueOf(self.name))
      impl.setDescription(self.description)
      impl.setInfo(dat2obj(self.info))
      return impl

def pn_condition_is_set(cond):
  return bool(cond.name)

def pn_condition_get_name(cond):
  return cond.name

def pn_condition_set_name(cond, name):
  cond.name = name

def pn_condition_get_description(cond):
  return cond.description

def pn_condition_set_description(cond, description):
  cond.description = description

def pn_condition_clear(cond):
  cond.name = None
  cond.description = None
  cond.info.clear()

def pn_condition_info(cond):
  return cond.info

class endpoint_wrapper:

  def __init__(self, impl):
    self.impl = impl
    self.context = None
    self.condition = pn_condition()
    self.remote_condition = pn_condition()

  def on_close(self):
    cond = self.condition.encode()
    if cond:
      self.impl.setCondition(cond)

def remote_condition(self):
  self.remote_condition.decode(self.impl.getRemoteCondition())
  return self.remote_condition

class pn_connection_wrapper(endpoint_wrapper):

  def __init__(self, impl):
    endpoint_wrapper.__init__(self, impl)
    self.properties = pn_data(0)
    self.offered_capabilities = pn_data(0)
    self.desired_capabilities = pn_data(0)

def pn_connection():
  return wrap(Proton.connection(), pn_connection_wrapper)

def set2mask(local, remote):
  mask = 0
  if local.contains(EndpointState.UNINITIALIZED):
    mask |= PN_LOCAL_UNINIT
  if local.contains(EndpointState.ACTIVE):
    mask |= PN_LOCAL_ACTIVE
  if local.contains(EndpointState.CLOSED):
    mask |= PN_LOCAL_CLOSED
  if remote.contains(EndpointState.UNINITIALIZED):
    mask |= PN_REMOTE_UNINIT
  if remote.contains(EndpointState.ACTIVE):
    mask |= PN_REMOTE_ACTIVE
  if remote.contains(EndpointState.CLOSED):
    mask |= PN_REMOTE_CLOSED
  return mask

def endpoint_state(impl):
  return set2mask(EnumSet.of(impl.getLocalState()),
                  EnumSet.of(impl.getRemoteState()))

def pn_connection_state(conn):
  return endpoint_state(conn.impl)

def pn_connection_condition(conn):
  return conn.condition

def pn_connection_remote_condition(conn):
  return remote_condition(conn)

def pn_connection_properties(conn):
  return conn.properties

def pn_connection_remote_properties(conn):
  return obj2dat(conn.impl.getRemoteProperties())

def pn_connection_offered_capabilities(conn):
  return conn.offered_capabilities

def pn_connection_remote_offered_capabilities(conn):
  return array2dat(conn.impl.getRemoteOfferedCapabilities(), PN_SYMBOL)

def pn_connection_desired_capabilities(conn):
  return conn.desired_capabilities

def pn_connection_remote_desired_capabilities(conn):
  return array2dat(conn.impl.getRemoteDesiredCapabilities(), PN_SYMBOL)

def pn_connection_get_context(conn):
  return conn.context

def pn_connection_set_context(conn, ctx):
  conn.context = ctx

def pn_connection_set_container(conn, name):
  conn.impl.setContainer(name)

def pn_connection_remote_container(conn):
  return conn.impl.getRemoteContainer()

def pn_connection_set_hostname(conn, name):
  conn.impl.setHostname(name)

def pn_connection_remote_hostname(conn):
  return conn.impl.getRemoteHostname()

def pn_connection_open(conn):
  props = dat2obj(conn.properties)
  offered = dat2obj(conn.offered_capabilities)
  desired = dat2obj(conn.desired_capabilities)
  if props:
    conn.impl.setProperties(props)
  if offered:
    conn.impl.setOfferedCapabilities(array(list(offered), Symbol))
  if desired:
    conn.impl.setDesiredCapabilities(array(list(desired), Symbol))
  conn.impl.open()

def pn_connection_close(conn):
  conn.on_close()
  conn.impl.close()

class pn_session_wrapper(endpoint_wrapper):
  pass

def pn_session(conn):
  return wrap(conn.impl.session(), pn_session_wrapper)

def pn_session_get_context(ssn):
  return ssn.context

def pn_session_set_context(ssn, ctx):
  ssn.context = ctx

def pn_session_state(ssn):
  return endpoint_state(ssn.impl)

def pn_session_get_incoming_capacity(ssn):
  return ssn.impl.getIncomingCapacity()

def pn_session_set_incoming_capacity(ssn, capacity):
  ssn.impl.setIncomingCapacity(capacity)

def pn_session_incoming_bytes(ssn):
  return ssn.impl.getIncomingBytes()

def pn_session_outgoing_bytes(ssn):
  return ssn.impl.getOutgoingBytes()

def pn_session_condition(ssn):
  return ssn.condition

def pn_session_remote_condition(ssn):
  return remote_condition(ssn)

def pn_session_open(ssn):
  ssn.impl.open()

def pn_session_close(ssn):
  ssn.on_close()
  ssn.impl.close()

def mask2set(mask):
  local = []
  remote = []
  if PN_LOCAL_UNINIT & mask:
    local.append(EndpointState.UNINITIALIZED)
  if PN_LOCAL_ACTIVE & mask:
    local.append(EndpointState.ACTIVE)
  if PN_LOCAL_CLOSED & mask:
    local.append(EndpointState.CLOSED)
  if PN_REMOTE_UNINIT & mask:
    remote.append(EndpointState.UNINITIALIZED)
  if PN_REMOTE_ACTIVE & mask:
    remote.append(EndpointState.ACTIVE)
  if PN_REMOTE_CLOSED & mask:
    remote.append(EndpointState.CLOSED)

  if local:
    local = EnumSet.of(*local)
  else:
    local = None
  if remote:
    remote = EnumSet.of(*remote)
  else:
    remote = None

  return local, remote

def pn_session_head(conn, mask):
  local, remote = mask2set(mask)
  return wrap(conn.impl.sessionHead(local, remote), pn_session_wrapper)

def pn_session_connection(ssn):
  return wrap(ssn.impl.getConnection(), pn_connection_wrapper)

def pn_sender(ssn, name):
  return wrap(ssn.impl.sender(name), pn_link_wrapper)

def pn_receiver(ssn, name):
  return wrap(ssn.impl.receiver(name), pn_link_wrapper)

def pn_session_free(ssn):
  ssn.impl = None

TERMINUS_TYPES_J2P = {
  Source: PN_SOURCE,
  Target: PN_TARGET,
  Coordinator: PN_COORDINATOR,
  None.__class__: PN_UNSPECIFIED
}

TERMINUS_TYPES_P2J = {
  PN_SOURCE: Source,
  PN_TARGET: Target,
  PN_COORDINATOR: Coordinator,
  PN_UNSPECIFIED: lambda: None
}

DURABILITY_P2J = {
  PN_NONDURABLE: TerminusDurability.NONE,
  PN_CONFIGURATION: TerminusDurability.CONFIGURATION,
  PN_DELIVERIES: TerminusDurability.UNSETTLED_STATE
}

DURABILITY_J2P = {
  TerminusDurability.NONE: PN_NONDURABLE,
  TerminusDurability.CONFIGURATION: PN_CONFIGURATION,
  TerminusDurability.UNSETTLED_STATE: PN_DELIVERIES
}

EXPIRY_POLICY_P2J = {
  PN_LINK_CLOSE: TerminusExpiryPolicy.LINK_DETACH,
  PN_SESSION_CLOSE: TerminusExpiryPolicy.SESSION_END,
  PN_CONNECTION_CLOSE: TerminusExpiryPolicy.CONNECTION_CLOSE,
  PN_NEVER: TerminusExpiryPolicy.NEVER
}

EXPIRY_POLICY_J2P = {
  TerminusExpiryPolicy.LINK_DETACH: PN_LINK_CLOSE,
  TerminusExpiryPolicy.SESSION_END: PN_SESSION_CLOSE,
  TerminusExpiryPolicy.CONNECTION_CLOSE: PN_CONNECTION_CLOSE,
  TerminusExpiryPolicy.NEVER: PN_NEVER
}

DISTRIBUTION_MODE_P2J = {
  PN_DIST_MODE_UNSPECIFIED: None,
  PN_DIST_MODE_COPY: Symbol.valueOf("copy"),
  PN_DIST_MODE_MOVE: Symbol.valueOf("move")
}

DISTRIBUTION_MODE_J2P = {
  None: PN_DIST_MODE_UNSPECIFIED,
  Symbol.valueOf("copy"): PN_DIST_MODE_COPY,
  Symbol.valueOf("move"): PN_DIST_MODE_MOVE
}

class pn_terminus:

  def __init__(self, type):
    self.type = type
    self.address = None
    self.durability = PN_NONDURABLE
    self.expiry_policy = PN_SESSION_CLOSE
    self.distribution_mode = PN_DIST_MODE_UNSPECIFIED
    self.timeout = 0
    self.dynamic = False
    self.properties = pn_data(0)
    self.capabilities = pn_data(0)
    self.outcomes = pn_data(0)
    self.filter = pn_data(0)

  def copy(self, src):
    self.type = src.type
    self.address = src.address
    self.durability = src.durability
    self.expiry_policy = src.expiry_policy
    self.timeout = src.timeout
    self.dynamic = src.dynamic
    self.properties = src.properties
    self.capabilities = src.capabilities
    self.outcomes = src.outcomes
    self.filter = src.filter

  def decode(self, impl):
    if impl is not None:
      self.type = TERMINUS_TYPES_J2P[impl.__class__]
      self.address = impl.getAddress()
      self.durability = DURABILITY_J2P[impl.getDurable()]
      self.expiry_policy = EXPIRY_POLICY_J2P[impl.getExpiryPolicy()]
      self.timeout = impl.getTimeout().longValue()
      self.dynamic = impl.getDynamic()
      obj2dat(impl.getDynamicNodeProperties(), self.properties)
      array2dat(impl.getCapabilities(), PN_SYMBOL, self.capabilities)
      if self.type == PN_SOURCE:
        self.distribution_mode = DISTRIBUTION_MODE_J2P[impl.getDistributionMode()]
        array2dat(impl.getOutcomes(), PN_SYMBOL, self.outcomes)
        obj2dat(impl.getFilter(), self.filter)

  def encode(self):
    impl = TERMINUS_TYPES_P2J[self.type]()
    if impl is not None:
      impl.setAddress(self.address)
      impl.setDurable(DURABILITY_P2J[self.durability])
      impl.setExpiryPolicy(EXPIRY_POLICY_P2J[self.expiry_policy])
      impl.setTimeout(UnsignedInteger.valueOf(self.timeout))
      impl.setDynamic(self.dynamic)
      props = dat2obj(self.properties)
      caps = dat2obj(self.capabilities)
      if props: impl.setDynamicNodeProperties(props)
      if caps:
        impl.setCapabilities(*array(list(caps), Symbol))
      if self.type == PN_SOURCE:
        impl.setDistributionMode(DISTRIBUTION_MODE_P2J[self.distribution_mode])
        outcomes = dat2obj(self.outcomes)
        filter = dat2obj(self.filter)
        if outcomes: impl.setOutcomes(outcomes)
        if filter: impl.setFilter(filter)
    return impl

def pn_terminus_get_type(terminus):
  return terminus.type

def pn_terminus_set_type(terminus, type):
  terminus.type = type
  return 0

def pn_terminus_get_address(terminus):
  return terminus.address

def pn_terminus_set_address(terminus, address):
  terminus.address = address
  return 0

def pn_terminus_get_durability(terminus):
  return terminus.durability

def pn_terminus_get_expiry_policy(terminus):
  return terminus.expiry_policy

def pn_terminus_set_timeout(terminus, timeout):
  terminus.timeout = timeout
  return 0

def pn_terminus_get_timeout(terminus):
  return terminus.timeout

def pn_terminus_get_distribution_mode(terminus):
  return terminus.distribution_mode

def pn_terminus_set_distribution_mode(terminus, mode):
  terminus.distribution_mode = mode
  return 0

def pn_terminus_is_dynamic(terminus):
  return terminus.dynamic

def pn_terminus_set_dynamic(terminus, dynamic):
  terminus.dynamic = dynamic
  return 0

def pn_terminus_properties(terminus):
  return terminus.properties

def pn_terminus_capabilities(terminus):
  return terminus.capabilities

def pn_terminus_outcomes(terminus):
  return terminus.outcomes

def pn_terminus_filter(terminus):
  return terminus.filter

def pn_terminus_copy(terminus, src):
  terminus.copy(src)
  return 0

class pn_link_wrapper(endpoint_wrapper):

  def __init__(self, impl):
    endpoint_wrapper.__init__(self, impl)
    self.source = pn_terminus(PN_SOURCE)
    self.remote_source = pn_terminus(PN_UNSPECIFIED)
    self.target = pn_terminus(PN_TARGET)
    self.remote_target = pn_terminus(PN_UNSPECIFIED)

  def on_open(self):
    self.impl.setSource(self.source.encode())
    self.impl.setTarget(self.target.encode())

def pn_link_get_context(link):
  return link.context

def pn_link_set_context(link, ctx):
  link.context = ctx

def pn_link_source(link):
  link.source.decode(link.impl.getSource())
  return link.source

def pn_link_remote_source(link):
  link.remote_source.decode(link.impl.getRemoteSource())
  return link.remote_source

def pn_link_target(link):
  link.target.decode(link.impl.getTarget())
  return link.target

def pn_link_remote_target(link):
  link.remote_target.decode(link.impl.getRemoteTarget())
  return link.remote_target

def pn_link_condition(link):
  return link.condition

def pn_link_remote_condition(link):
  return remote_condition(link)

SND_SETTLE_MODE_P2J = {
  PN_SND_UNSETTLED: SenderSettleMode.UNSETTLED,
  PN_SND_SETTLED: SenderSettleMode.SETTLED,
  PN_SND_MIXED: SenderSettleMode.MIXED,
  None: None
}

SND_SETTLE_MODE_J2P = {
  SenderSettleMode.UNSETTLED: PN_SND_UNSETTLED,
  SenderSettleMode.SETTLED: PN_SND_SETTLED,
  SenderSettleMode.MIXED: PN_SND_MIXED,
  None: None
}

def pn_link_set_snd_settle_mode(link, mode):
  link.impl.setSenderSettleMode(SND_SETTLE_MODE_P2J[mode])

def pn_link_snd_settle_mode(link):
  return SND_SETTLE_MODE_J2P[link.impl.getSenderSettleMode()]

def pn_link_remote_snd_settle_mode(link):
  return SND_SETTLE_MODE_J2P[link.impl.getRemoteSenderSettleMode()]

RCV_SETTLE_MODE_P2J = {
  PN_RCV_FIRST: ReceiverSettleMode.FIRST,
  PN_RCV_SECOND: ReceiverSettleMode.SECOND,
  None: None
}

RCV_SETTLE_MODE_J2P = {
  ReceiverSettleMode.FIRST: PN_RCV_FIRST,
  ReceiverSettleMode.SECOND: PN_RCV_SECOND,
  None: None
}

def pn_link_set_rcv_settle_mode(link, mode):
  link.impl.setReceiverSettleMode(RCV_SETTLE_MODE_P2J[mode])

def pn_link_rcv_settle_mode(link):
  return RCV_SETTLE_MODE_J2P[link.impl.getReceiverSettleMode()]

def pn_link_remote_rcv_settle_mode(link):
  return RCV_SETTLE_MODE_J2P[link.impl.getRemoteReceiverSettleMode()]

def pn_link_is_sender(link):
  return isinstance(link.impl, Sender)

def pn_link_head(conn, mask):
  local, remote = mask2set(mask)
  return wrap(conn.impl.linkHead(local, remote), pn_link_wrapper)

def pn_link_next(link, mask):
  local, remote = mask2set(mask)
  return wrap(link.impl.next(local, remote), pn_link_wrapper)

def pn_link_session(link):
  return wrap(link.impl.getSession(), pn_session_wrapper)

def pn_link_state(link):
  return endpoint_state(link.impl)

def pn_link_name(link):
  return link.impl.getName()

def pn_link_open(link):
  link.on_open()
  link.impl.open()

def pn_link_close(link):
  link.on_close()
  link.impl.close()

def pn_link_flow(link, n):
  link.impl.flow(n)

def pn_link_drain(link, n):
  link.impl.drain(n)

def pn_link_drained(link):
  return link.impl.drained()

def pn_link_draining(link):
  return link.impl.draining()

def pn_link_credit(link):
  return link.impl.getCredit()

def pn_link_queued(link):
  return link.impl.getQueued()

def pn_link_unsettled(link):
  return link.impl.getUnsettled()

def pn_link_send(link, bytes):
  return link.impl.send(array(bytes, 'b'), 0, len(bytes))

def pn_link_recv(link, limit):
  ary = zeros(limit, 'b')
  n = link.impl.recv(ary, 0, limit)
  if n >= 0:
    bytes = ary[:n].tostring()
  else:
    bytes = None
  return n, bytes

def pn_link_advance(link):
  return link.impl.advance()

def pn_link_current(link):
  return wrap(link.impl.current(), pn_delivery_wrapper)

def pn_link_free(link):
  link.impl = None

def pn_work_head(conn):
  return wrap(conn.impl.getWorkHead(), pn_delivery_wrapper)

def pn_work_next(dlv):
  return wrap(dlv.impl.getWorkNext(), pn_delivery_wrapper)

DELIVERY_STATES = {
  Received: PN_RECEIVED,
  Accepted: PN_ACCEPTED,
  Rejected: PN_REJECTED,
  Released: PN_RELEASED,
  Modified: PN_MODIFIED,
  None.__class__: 0
  }

DISPOSITIONS = {
  PN_RECEIVED: Received,
  PN_ACCEPTED: Accepted,
  PN_REJECTED: Rejected,
  PN_RELEASED: Released,
  PN_MODIFIED: Modified,
  0: lambda: None
}

class pn_disposition:

  def __init__(self):
    self.type = 0
    self.data = pn_data(0)
    self.failed = False
    self.undeliverable = False
    self.annotations = pn_data(0)
    self.condition = pn_condition()
    self.section_number = 0
    self.section_offset = 0

  def decode(self, impl):
    self.type = DELIVERY_STATES[impl.__class__]

    if self.type == PN_REJECTED:
      self.condition.decode(impl.getError())
    else:
      pn_condition_clear(self.condition)

    if self.type == PN_MODIFIED:
      self.failed = impl.getDeliveryFailed()
      self.undeliverable = impl.getUndeliverableHere()
      obj2dat(impl.getMessageAnnotations(), self.annotations)
    else:
      self.failed = False
      self.undeliverable = False
      pn_data_clear(self.annotations)

    if self.type == PN_RECEIVED:
      self.section_number = impl.getSectionNumber().longValue()
      self.section_offset = impl.getSectionOffset().longValue()
    else:
      self.section_number = 0
      self.section_offset = 0

    self.data.clear()
    if impl:
      # XXX
      #self.data.putObject(impl)
      pass
    self.data.rewind()

  def encode(self):
    if self.type not in DISPOSITIONS:
      raise Skipped()
    impl = DISPOSITIONS[self.type]()

    if impl is None:
      return impl

    if self.type == PN_REJECTED:
      impl.setError(self.condition.encode())

    if self.type == PN_MODIFIED:
      impl.setDeliveryFailed(self.failed)
      impl.setUndeliverableHere(self.undeliverable)
      ann = dat2obj(self.annotations)
      if ann: impl.setMessageAnnotations(ann)

    if self.type == PN_RECEIVED:
      if self.section_number:
        impl.setSectionNumber(UnsignedInteger.valueOf(self.section_number))
      if self.section_offset:
        impl.setSectionOffset(UnsignedLong.valueOf(self.section_offset))

    return impl

def pn_disposition_type(dsp):
  return dsp.type

def pn_disposition_is_failed(dsp):
  return dsp.failed

def pn_disposition_set_failed(dsp, failed):
  dsp.failed = failed

def pn_disposition_is_undeliverable(dsp):
  return dsp.undeliverable

def pn_disposition_set_undeliverable(dsp, undeliverable):
  dsp.undeliverable = undeliverable

def pn_disposition_data(dsp):
  return dsp.data

def pn_disposition_annotations(dsp):
  return dsp.annotations

def pn_disposition_condition(dsp):
  return dsp.condition

def pn_disposition_get_section_number(dsp):
  return dsp.section_number

def pn_disposition_set_section_number(dsp, number):
  dsp.section_number = number

def pn_disposition_get_section_offset(dsp):
  return dsp.section_offset

def pn_disposition_set_section_offset(dsp, offset):
  dsp.section_offset = offset

class pn_delivery_wrapper:

  def __init__(self, impl):
    self.impl = impl
    self.context = None
    self.local = pn_disposition()
    self.remote = pn_disposition()

def pn_delivery(link, tag):
  return wrap(link.impl.delivery(array(tag, 'b')), pn_delivery_wrapper)

def pn_delivery_tag(dlv):
  return dlv.impl.getTag().tostring()

def pn_delivery_get_context(dlv):
  return dlv.context

def pn_delivery_set_context(dlv, ctx):
  dlv.context = ctx

def pn_delivery_pending(dlv):
  return dlv.impl.pending()

def pn_delivery_writable(dlv):
  return dlv.impl.isWritable()

def pn_delivery_readable(dlv):
  return dlv.impl.isReadable()

def pn_delivery_updated(dlv):
  return dlv.impl.isUpdated()

def pn_delivery_settled(dlv):
  return dlv.impl.remotelySettled()

def pn_delivery_local(dlv):
  dlv.local.decode(dlv.impl.getLocalState())
  return dlv.local

def pn_delivery_local_state(dlv):
  dlv.local.decode(dlv.impl.getLocalState())
  return dlv.local.type

def pn_delivery_remote(dlv):
  dlv.remote.decode(dlv.impl.getRemoteState())
  return dlv.remote

def pn_delivery_remote_state(dlv):
  dlv.remote.decode(dlv.impl.getRemoteState())
  return dlv.remote.type

def pn_delivery_update(dlv, state):
  dlv.local.type = state
  dlv.impl.disposition(dlv.local.encode())

def pn_delivery_link(dlv):
  return wrap(dlv.impl.getLink(), pn_link_wrapper)

def pn_delivery_settle(dlv):
  dlv.impl.settle()

class pn_transport_wrapper:

  def __init__(self, impl):
    self.impl = impl
    self.error = pn_error(0, None)

def pn_transport():
  return wrap(Proton.transport(), pn_transport_wrapper)

def pn_transport_get_max_frame(trans):
  return trans.impl.getMaxFrameSize()

def pn_transport_set_max_frame(trans, value):
  trans.impl.setMaxFrameSize(value)

def pn_transport_get_remote_max_frame(trans):
  return trans.impl.getRemoteMaxFrameSize()

def pn_transport_set_idle_timeout(trans, value):
  raise Skipped()

def pn_transport_set_channel_max(trans, n):
  trans.impl.setChannelMax(n)

def pn_transport_get_channel_max(trans):
  return trans.impl.getChannelMax()

def pn_transport_remote_channel_max(trans):
  return trans.impl.getRemoteChannelMax()

def pn_transport_bind(trans, conn):
  trans.impl.bind(conn.impl)
  return 0

def pn_transport_trace(trans, n):
  # XXX
  pass

def pn_transport_pending(trans):
  try:
    return trans.impl.pending()
  except TransportException, e:
    return trans.error.set(PN_ERR, str(e))

def pn_transport_peek(trans, size):
  size = min(trans.impl.pending(), size)
  ba = zeros(size, 'b')
  if size:
    bb = trans.impl.head()
    bb.get(ba)
  return 0, ba.tostring()

def pn_transport_pop(trans, size):
  trans.impl.pop(size)

def pn_transport_capacity(trans):
  return trans.impl.capacity()

def pn_transport_push(trans, input):
  cap = pn_transport_capacity(trans)
  if cap < 0:
    return cap
  elif len(input) > cap:
    return PN_OVERFLOW
  else:
    bb = trans.impl.tail()
    bb.put(array(input, 'b'))
    try:
      trans.impl.process()
      return 0
    except TransportException, e:
      trans.error = pn_error(PN_ERR, str(e))
      return PN_ERR

def pn_transport_close_head(trans):
  try:
    trans.impl.close_head()
    return 0
  except TransportException, e:
    trans.error = pn_error(PN_ERR, str(e))
    return PN_ERR

def pn_transport_close_tail(trans):
  try:
    trans.impl.close_tail()
    return 0
  except TransportException, e:
    trans.error = pn_error(PN_ERR, str(e))
    return PN_ERR

def pn_transport_error(trans):
  return trans.error

from org.apache.qpid.proton.engine import Event

PN_EVENT_CATEGORY_PROTOCOL = Event.Category.PROTOCOL

PN_CONNECTION_LOCAL_STATE = Event.Type.CONNECTION_LOCAL_STATE
PN_CONNECTION_REMOTE_STATE = Event.Type.CONNECTION_REMOTE_STATE
PN_SESSION_LOCAL_STATE = Event.Type.SESSION_LOCAL_STATE
PN_SESSION_REMOTE_STATE = Event.Type.SESSION_REMOTE_STATE
PN_LINK_LOCAL_STATE = Event.Type.LINK_LOCAL_STATE
PN_LINK_REMOTE_STATE = Event.Type.LINK_REMOTE_STATE
PN_LINK_FLOW = Event.Type.LINK_FLOW
PN_DELIVERY = Event.Type.DELIVERY
PN_TRANSPORT = Event.Type.TRANSPORT

def pn_collector():
  return Proton.collector()

def pn_connection_collect(conn, coll):
  conn.impl.collect(coll)

def pn_collector_peek(coll):
  return coll.peek()

def pn_collector_pop(coll):
  coll.pop()

def pn_collector_free(coll):
  pass

def pn_event_connection(event):
  return wrap(event.getConnection(), pn_connection_wrapper)

def pn_event_session(event):
  return wrap(event.getSession(), pn_session_wrapper)

def pn_event_link(event):
  return wrap(event.getLink(), pn_link_wrapper)

def pn_event_delivery(event):
  return wrap(event.getDelivery(), pn_delivery_wrapper)

def pn_event_transport(event):
  return wrap(event.getTransport(), pn_transport_wrapper)

def pn_event_class(event):
  return event.getClass()

def pn_event_type(event):
  return event.getType()

def pn_event_type_name(etype):
  return str(etype)

def pn_event_category(event):
  return event.getCategory()
