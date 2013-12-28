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

from uuid import UUID
import uuid
try:
  bytes()
except NameError:
  bytes = str

from org.apache.qpid.proton import Proton, ProtonUnsupportedOperationException
from org.apache.qpid.proton import InterruptException as Interrupt
from org.apache.qpid.proton import TimeoutException as Timeout
from org.apache.qpid.proton.engine import \
    Transport as JTransport, Sender as JSender, Receiver as JReceiver, \
    Sasl, SslDomain as JSslDomain, \
    EndpointState, TransportException

from org.apache.qpid.proton.message import \
    MessageFormat, Message as JMessage
from org.apache.qpid.proton.codec import \
    Data as JData
from org.apache.qpid.proton.messenger import MessengerException, Status
from org.apache.qpid.proton.amqp.transport import ErrorCondition, SenderSettleMode, ReceiverSettleMode
from org.apache.qpid.proton.amqp.messaging import Source, Target, Accepted, \
    Rejected, Received, Modified, Released, AmqpValue
from org.apache.qpid.proton.amqp import UnsignedInteger, UnsignedLong, UnsignedByte, UnsignedShort, Symbol, \
    Decimal32, Decimal64, Decimal128
from jarray import zeros, array
from java.util import EnumSet, UUID as JUUID, Date as JDate, HashMap
from java.nio import ByteBuffer
from java.lang import Character as JCharacter, String as JString, Integer as JInteger
from java.lang import NoClassDefFoundError



class Constant(object):

  def __init__(self, name):
    self.name = name

  def __repr__(self):
    return self.name

class Skipped(Exception):
  skipped = True

PENDING = "PENDING"
ACCEPTED = "ACCEPTED"
REJECTED = "REJECTED"
RELEASED = "RELEASED"
SETTLED = "SETTLED"

STATUSES = {
  Status.ACCEPTED: ACCEPTED,
  Status.REJECTED: REJECTED,
  Status.PENDING: PENDING,
  Status.RELEASED: RELEASED,
  Status.SETTLED: SETTLED,
  Status.UNKNOWN: None
  }

MANUAL = "MANUAL"
AUTOMATIC = "AUTOMATIC"

API_LANGUAGE = "Java"
IMPLEMENTATION_LANGUAGE = "C"
if Proton.getDefaultImplementationType().name() == "PROTON_J":
  IMPLEMENTATION_LANGUAGE = "Java"


class Endpoint(object):

  LOCAL_UNINIT = 1
  LOCAL_ACTIVE = 2
  LOCAL_CLOSED = 4
  REMOTE_UNINIT = 8
  REMOTE_ACTIVE = 16
  REMOTE_CLOSED = 32

  def __init__(self):
    self.condition = None

  @property
  def remote_condition(self):
    return Condition(impl = self.impl.getRemoteCondition())


  @property
  def state(self):
    local = self.impl.getLocalState()
    remote = self.impl.getRemoteState()

    result = 0

    if (local == EndpointState.UNINITIALIZED):
      result = result | self.LOCAL_UNINIT
    elif (local == EndpointState.ACTIVE):
      result = result | self.LOCAL_ACTIVE
    elif (local == EndpointState.CLOSED):
      result = result | self.LOCAL_CLOSED

    if (remote == EndpointState.UNINITIALIZED):
      result = result | self.REMOTE_UNINIT
    elif (remote == EndpointState.ACTIVE):
      result = result | self.REMOTE_ACTIVE
    elif (remote == EndpointState.CLOSED):
      result = result | self.REMOTE_CLOSED

    return result

  def _enums(self, mask):
    local = []
    if (self.LOCAL_UNINIT | mask):
      local.append(EndpointState.UNINITIALIZED)
    if (self.LOCAL_ACTIVE | mask):
      local.append(EndpointState.ACTIVE)
    if (self.LOCAL_CLOSED | mask):
      local.append(EndpointState.CLOSED)

    remote = []
    if (self.REMOTE_UNINIT | mask):
      remote.append(EndpointState.UNINITIALIZED)
    if (self.REMOTE_ACTIVE | mask):
      remote.append(EndpointState.ACTIVE)
    if (self.REMOTE_CLOSED | mask):
      remote.append(EndpointState.CLOSED)

    return EnumSet.of(*local), EnumSet.of(*remote)


  def open(self):
    self.impl.open()

  def close(self):
    if self.condition is not None:
      self.impl.setCondition(self.condition.impl)
    self.impl.close()

class Condition(object):

  def __init__(self, name=None, description=None, info=None, impl=None):
    if impl is None:
      impl = ErrorCondition(Symbol.valueOf(name), description)
      if info is not None:
        impl.setInfo(info)
    self.impl = impl

  def _get_name(self):
    c = self.impl.getCondition()
    if c is not None:
      return c.toString()
  def _set_name(self, n):
    self.impl.setCondition(Symbol.valueOf(n))
  name = property(_get_name, _set_name)

  def _get_description(self):
    return self.impl.getDescription()
  def _set_description(self, d):
    self.impl.setDescription(d)
  description = property(_get_description, _set_description)

  def _get_info(self):
    return self.impl.getInfo()
  def _set_info(self, i):
    self.impl.setInfo(i)
  info = property(_get_info, _get_description)

  def __repr__(self):
    return "Condition(%s)" % ", ".join([repr(x) for x in
                                        (self.name, self.description, self.info)
                                        if x])

  def __eq__(self, o):
    if not isinstance(o, Condition): return False
    return self.impl.equals(o.impl)

  def _2J(self):
    return self.impl

def wrap_connection(impl):
  if impl:
    return impl.getContext()
  else:
    return None

class Connection(Endpoint):

  def __init__(self):
    Endpoint.__init__(self)
    self.impl = Proton.connection()
    self.impl.setContext(self)
    self.desired_capabilities = None
    self.offered_capabilities = None
    self.properties = None

  @property
  def writable(self):
    raise ProtonUnsupportedOperationException("Connection.writable")

  def session(self):
    return wrap_session(self.impl.session())

  def session_head(self, mask):
    return wrap_session(self.impl.sessionHead(*self._enums(mask)))

  def link_head(self, mask):
    return wrap_link(self.impl.linkHead(*self._enums(mask)))

  @property
  def work_head(self):
    return wrap_delivery(self.impl.getWorkHead())

  def _get_container(self):
    return self.impl.getContainer()
  def _set_container(self, container):
    self.impl.setContainer(container)
  container = property(_get_container, _set_container)


  def _get_hostname(self):
      return self.impl.getHostname()
  def _set_hostname(self, hostname):
      self.impl.setHostname(hostname)
  hostname = property(_get_hostname, _set_hostname)

  def _get_remote_container(self):
    return self.impl.getRemoteContainer()
  def _set_remote_container(self, container):
    self.impl.setRemoteContainer(container)
  remote_container = property(_get_remote_container, _set_remote_container)


  def _get_remote_hostname(self):
      return self.impl.getRemoteHostname()
  def _set_remote_hostname(self, hostname):
      self.impl.setRemoteHostname(hostname)
  remote_hostname = property(_get_remote_hostname, _set_remote_hostname)

  @property
  def remote_offered_capabilities(self):
      return convertToPyArray(Data.SYMBOL, self.impl.getRemoteOfferedCapabilities(),symbol)

  @property
  def remote_desired_capabilities(self):
      return convertToPyArray(Data.SYMBOL, self.impl.getRemoteDesiredCapabilities(),symbol)

  @property
  def remote_properties(self):
    return J2PY(self.impl.getRemoteProperties());

  def open(self):
    self.impl.setOfferedCapabilities(PY2J(self.offered_capabilities))
    self.impl.setDesiredCapabilities(PY2J(self.desired_capabilities))
    self.impl.setProperties(PY2J(self.properties))
    Endpoint.open(self)

def wrap_session(impl):
  # XXX
  if impl: return Session(impl)

class Session(Endpoint):

  def __init__(self, impl):
    Endpoint.__init__(self)
    self.impl = impl

  @property
  def connection(self):
    return wrap_connection(self.impl.getConnection())

  def sender(self, name):
    return wrap_link(self.impl.sender(name))

  def receiver(self, name):
    return wrap_link(self.impl.receiver(name))

  def _get_incoming_capacity(self):
    return self.impl.getIncomingCapacity()

  def _set_incoming_capacity(self, capacity):
    self.impl.setIncomingCapacity(capacity)

  incoming_capacity = property(_get_incoming_capacity, _set_incoming_capacity)

  @property
  def outgoing_bytes(self):
    return self.impl.getOutgoingBytes()

  @property
  def incoming_bytes(self):
    return self.impl.getIncomingBytes()

def wrap_link(impl):
  if impl is None: return None
  elif isinstance(impl, JSender):
    return Sender(impl)
  elif isinstance(impl, JReceiver):
    return Receiver(impl)
  else:
    raise Exception("unknown type")

class Link(Endpoint):

  SND_UNSETTLED = SenderSettleMode.UNSETTLED
  SND_SETTLED = SenderSettleMode.SETTLED
  SND_MIXED = SenderSettleMode.MIXED

  RCV_FIRST = ReceiverSettleMode.FIRST
  RCV_SECOND = ReceiverSettleMode.SECOND

  def __init__(self, impl):
    Endpoint.__init__(self)
    self.impl = impl

  @property
  def source(self):
    if self.impl.getSource() is None:
      self.impl.setSource(Source())
    return Terminus(self.impl.getSource())

  @property
  def target(self):
    if self.impl.getTarget() is None:
      self.impl.setTarget(Target())
    return Terminus(self.impl.getTarget())

  @property
  def remote_source(self):
    return Terminus(self.impl.getRemoteSource())
  @property
  def remote_target(self):
    return Terminus(self.impl.getRemoteTarget())

  @property
  def session(self):
    return wrap_session(self.impl.getSession())

  def delivery(self, tag):
    return wrap_delivery(self.impl.delivery(tag))

  @property
  def current(self):
    return wrap_delivery(self.impl.current())

  def advance(self):
    return self.impl.advance()

  @property
  def unsettled(self):
    return self.impl.getUnsettled()

  @property
  def credit(self):
    return self.impl.getCredit()

  @property
  def available(self):
    raise ProtonUnsupportedOperationException("Link.available")

  @property
  def queued(self):
    return self.impl.getQueued()

  def next(self, mask):
    return wrap_link(self.impl.next(*self._enums(mask)))

  @property
  def name(self):
      return self.impl.getName()

  @property
  def remote_snd_settle_mode(self):
    return self.impl.getRemoteSenderSettleMode()

  @property
  def remote_rcv_settle_mode(self):
    return self.impl.getRemoteReceiverSettleMode()

  def _get_snd_settle_mode(self):
    return self.impl.getSenderSettleMode()
  def _set_snd_settle_mode(self, mode):
    self.impl.setSenderSettleMode(mode)
  snd_settle_mode = property(_get_snd_settle_mode, _set_snd_settle_mode)

  def _get_rcv_settle_mode(self):
    return self.impl.getReceiverSettleMode()
  def _set_rcv_settle_mode(self, mode):
    self.impl.setReceiverSettleMode(mode)
  rcv_settle_mode = property(_get_rcv_settle_mode, _set_rcv_settle_mode)

  def drained(self):
    return self.impl.drained()


class DataDummy:

  def format(self):
    pass

  def put_array(self, *args, **kwargs):
    raise ProtonUnsupportedOperationException("Data.put_array")

class Terminus(object):

  UNSPECIFIED = None

  DIST_MODE_UNSPECIFIED = None
  DIST_MODE_COPY = "copy"
  DIST_MODE_MOVE = "move"

  def __init__(self, impl):
    self.impl = impl
    self.type = None
    self.timeout = None
    self.durability = None
    self.expiry_policy = None
    self.properties = DataDummy()
    self.outcomes = DataDummy()
    self.filter = DataDummy()
    self.capabilities = DataDummy()

  def _get_address(self):
    return self.impl.getAddress()
  def _set_address(self, address):
    self.impl.setAddress(address)
  address = property(_get_address, _set_address)

  def _get_timeout(self):
    return self.impl.getTimeout()
  def _set_timeout(self, t):
    if t is not None:
      t = UnsignedInteger(t)
    return self.impl.setTimeout(t)
  timeout = property(_get_timeout, _set_timeout)

  def _is_dynamic(self):
    return self.impl.getDynamic()
  def _set_dynamic(self, dynamic):
    self.impl.setDynamic(dynamic)
  dynamic = property(_is_dynamic, _set_dynamic)

  def _get_distribution_mode(self):
    if isinstance(self.impl, Source):
      sym = self.impl.getDistributionMode()
      if sym is None:
        return self.DIST_MODE_UNSPECIFIED
      else:
        return sym.toString()
    else:
      return self.DIST_MODE_UNSPECIFIED
  def _set_distribution_mode(self, mode):
    if isinstance(self.impl, Source):
      if mode in [None, "copy", "move"]:
        self.impl.setDistributionMode(Symbol.valueOf(mode))
      else:
        self.impl.setDistributionMode(None)
  distribution_mode = property(_get_distribution_mode, _set_distribution_mode)

  def copy(self, src):
    self.address = src.address
    self.timeout = src.timeout
    self.dynamic = src.dynamic
    self.distribution_mode = src.distribution_mode

class Sender(Link):

  def offered(self, n):
    raise ProtonUnsupportedOperationException("Sender.offered")

  def send(self, bytes):
    return self.impl.send(bytes, 0, len(bytes))

class Receiver(Link):

  def flow(self, n):
    self.impl.flow(n)

  def drain(self, n):
    self.impl.drain(n)

  def draining(self):
    return self.impl.draining()

  def recv(self, size):
    output = zeros(size, "b")
    n = self.impl.recv(output, 0, size)
    if n >= 0:
      return output.tostring()[:n]
    elif n == JTransport.END_OF_STREAM:
      return None
    else:
      raise Exception(n)

class Disposition(object):

  RECEIVED = 0x23
  ACCEPTED = 0x24
  REJECTED = 0x25
  RELEASED = 0x26
  MODIFIED = 0x27

  def __init__(self):
    self.type = 0
    self._received = None
    self._accepted = None
    self._rejected = None
    self._released = None
    self._modified = None

  def _get_section_number(self):
    if self._received:
      return J2PY(self._received.getSectionNumber())
    else:
      return 0
  def _set_section_number(self, n):
    if not self._received:
      self._received = Received()
    self._received.setSectionNumber(UnsignedInteger(n))
  section_number = property(_get_section_number, _set_section_number)

  def _get_section_offset(self):
    if self._received:
      return J2PY(self._received.getSectionOffset())
    else:
      return 0
  def _set_section_offset(self, n):
    if not self._received:
      self._received = Received()
    self._received.setSectionOffset(UnsignedLong(n))
  section_offset = property(_get_section_offset, _set_section_offset)

  def _get_failed(self):
    if self._modified:
      return self._modified.getDeliveryFailed()
    else:
      return False
  def _set_failed(self, b):
    if not self._modified:
      self._modified = Modified()
    self._modified.setDeliveryFailed(b)
  failed = property(_get_failed, _set_failed)

  def _get_undeliverable(self):
    if self._modified:
      return self._modified.getUndeliverableHere()
    else:
      return False
  def _set_undeliverable(self, b):
    if not self._modified:
      self._modified = Modified()
    self._modified.setUndeliverableHere(b)
  undeliverable = property(_get_undeliverable, _set_undeliverable)

  def _get_data(self):
    return None
  def _set_data(self, obj):
    raise Skipped()
  data = property(_get_data, _set_data)

  def _get_annotations(self):
    if self._modified:
      return J2PY(self._modified.getMessageAnnotations())
    else:
      return None
  def _set_annotations(self, obj):
    if not self._modified:
      self._modified = Modified()
    self._modified.setMessageAnnotations(PY2J(obj))
  annotations = property(_get_annotations, _set_annotations)

  def _get_condition(self):
    if self._rejected:
      return Condition(impl = self._rejected.getError())
    else:
      return None
  def _set_condition(self, obj):
    if not self._rejected:
      self._rejected = Rejected()
    self._rejected.setError(obj._2J())
  condition = property(_get_condition, _set_condition)

  def _as_received(self):
    if self._received is None:
      self._received = Received()
    return self._received

  def _as_accepted(self):
    if self._accepted is None:
      self._accepted = Accepted.getInstance()
    return self._accepted

  def _as_rejected(self):
    if self._rejected is None:
      self._rejected = Rejected()
    return self._rejected

  def _as_released(self):
    if self._released is None:
      self._released = Released.getInstance()
    return self._released

  def _as_modified(self):
    if self._modified is None:
      self._modified = Modified()
    return self._modified

  PY2J = {
    RECEIVED: _as_received,
    ACCEPTED: _as_accepted,
    REJECTED: _as_rejected,
    RELEASED: _as_released,
    MODIFIED: _as_modified
  }

  def _2J(self):
    return self.PY2J[self.type](self)

  def _from_received(self, s):
    self.type = self.RECEIVED
    self._received = s

  def _from_accepted(self, s):
    self.type = self.ACCEPTED
    self._accepted = s

  def _from_rejected(self, s):
    self.type = self.REJECTED
    self._rejected = s

  def _from_released(self, s):
    self.type = self.RELEASED
    self._released = s

  def _from_modified(self, s):
    self.type = self.MODIFIED
    self._modified = s

  J2PY = {
    Received: _from_received,
    Accepted: _from_accepted,
    Rejected: _from_rejected,
    Released: _from_released,
    Modified: _from_modified
    }

  def _2PY(self, impl):
    self.J2PY[type(impl)](self, impl)

def wrap_delivery(impl):
  if impl: return Delivery(impl)

class Delivery(object):

  RECEIVED = Disposition.RECEIVED
  ACCEPTED = Disposition.ACCEPTED
  REJECTED = Disposition.REJECTED
  RELEASED = Disposition.RELEASED
  MODIFIED = Disposition.MODIFIED

  def __init__(self, impl):
    self.impl = impl
    self.local = Disposition()

  @property
  def tag(self):
    return self.impl.getTag().tostring()

  @property
  def writable(self):
    return self.impl.isWritable()

  @property
  def readable(self):
    return self.impl.isReadable()

  @property
  def updated(self):
    return self.impl.isUpdated()

  def update(self, disp):
    self.local.type = disp
    self.impl.disposition(self.local._2J())

  @property
  def remote(self):
    d = Disposition()
    d._2PY(self.impl.getRemoteState())
    return d

  @property
  def remote_state(self):
    return self.remote.type

  @property
  def local_state(self):
    return self.local.type

  def settle(self):
    self.impl.settle()

  @property
  def settled(self):
    return self.impl.remotelySettled()

  @property
  def work_next(self):
    return wrap_delivery(self.impl.getWorkNext())

  @property
  def pending(self):
    return self.impl.pending()

class Transport(object):

  TRACE_OFF = 0
  TRACE_RAW = 1
  TRACE_FRM = 2
  TRACE_DRV = 4

  def __init__(self):
    self.impl = Proton.transport()
    self._ssl = None
    self._sasl = None

  def __del__(self):
    if hasattr(self, ".impl") and self.impl:
      pn_transport_free(self.impl)
      if hasattr(self, "_sasl") and self._sasl:
        # pn_transport_free deallocs the C sasl associated with the
        # transport, so erase the reference if a SASL object was used.
        self._sasl._sasl = None
        self._sasl = None
      if hasattr(self, "_ssl") and self._ssl:
        # ditto the owned c SSL object
        self._ssl._ssl = None
        self._ssl = None
      del self._trans

  def trace(self, mask):
    # XXX: self.impl.trace(mask)
    pass

  def bind(self, connection):
    self.impl.bind(connection.impl)

  def capacity(self):
    return self.impl.capacity()

  def push(self, bytes):
    input_buffer = self.impl.tail()
    input_buffer.put(bytes)
    self.impl.process()

  def close_tail(self):
    self.impl.close_tail()

  def pending(self):
    return self.impl.pending()

  def peek(self, size):
    output_buffer = self.impl.head()
    output_length = min(size, output_buffer.remaining())
    output = zeros(output_length, "b")
    output_buffer.mark()
    output_buffer.get(output)
    output_buffer.reset()
    return output.tostring()

  def pop(self, size):
    self.impl.pop(size)

  def close_head(self):
    self.impl.close_head()

  def output(self, size):
    p = self.pending()
    if p < 0:
      return None
    else:
      out = self.peek(min(size, p))
      self.pop(len(out))
      return out

  def input(self, bytes):
    if not bytes:
      self.close_tail()
      return None
    else:
      c = self.capacity()
      if (c < 0):
        return None
      trimmed = bytes[:c]
      self.push(trimmed)
      return len(trimmed)

  def _get_max_frame_size(self):
    return self.impl.getMaxFrameSize()


  def _set_max_frame_size(self, value):
    self.impl.setMaxFrameSize(value)

  max_frame_size = property(_get_max_frame_size, _set_max_frame_size,
                            doc="""
Sets the maximum size for received frames (in bytes).
""")

  @property
  def remote_max_frame_size(self):
    return self.impl.getRemoteMaxFrameSize()

  # AMQP 1.0 idle-time-out
  def _get_idle_timeout(self):
    #return pn_transport_get_idle_timeout(self._trans)
    raise ProtonUnsupportedOperationException("Transport.idle_timeout")

  def _set_idle_timeout(self, value):
    #pn_transport_set_idle_timeout(self._trans, value)
    raise ProtonUnsupportedOperationException("Transport.idle_timeout")

  idle_timeout = property(_get_idle_timeout, _set_idle_timeout,
                          doc="""
The idle timeout of the connection (in milliseconds).
""")

  @property
  def remote_idle_timeout(self):
    #return pn_transport_get_remote_idle_timeout(self._trans)
    raise ProtonUnsupportedOperationException("Transport.remote_idle_timeout")

  @property
  def frames_output(self):
    #return pn_transport_get_frames_output(self._trans)
    raise ProtonUnsupportedOperationException("Transport.frames_output")

  @property
  def frames_input(self):
    #return pn_transport_get_frames_input(self._trans)
    raise ProtonUnsupportedOperationException("Transport.frames_input")

  def sasl(self):
    # SASL factory (singleton for this transport)
    if not self._sasl:
      self._sasl = SASL(self)
    return self._sasl

  def ssl(self, domain=None, session_details=None):
    # SSL factory (singleton for this transport)
    if not self._ssl:
      self._ssl = SSL(self, domain, session_details)
    return self._ssl

class UnmappedType:

  def __init__(self, msg):
    self.msg = msg

  def __repr__(self):
    return "UnmappedType(%s)" % self.msg


class ulong(long):

  def __repr__(self):
    return "ulong(%s)" % long.__repr__(self)

class timestamp(long):

  def __repr__(self):
    return "timestamp(%s)" % long.__repr__(self)

class symbol(unicode):

  def __repr__(self):
    return "symbol(%s)" % unicode.__repr__(self)

class char(unicode):

  def __repr__(self):
    return "char(%s)" % unicode.__repr__(self)

class Described(object):

  def __init__(self, descriptor, value):
    self.descriptor = descriptor
    self.value = value

  def __repr__(self):
    return "Described(%r, %r)" % (self.descriptor, self.value)

  def __eq__(self, o):
    if isinstance(o, Described):
      return self.descriptor == o.descriptor and self.value == o.value
    else:
      return False

UNDESCRIBED = Constant("UNDESCRIBED")

class Array(object):

  def __init__(self, descriptor, type, *elements):
    self.descriptor = descriptor
    self.type = type
    self.elements = elements

  def __repr__(self):
    if self.elements:
      els = ", %s"  % (", ".join(map(repr, self.elements)))
    else:
      els = ""
    return "Array(%r, %r%s)" % (self.descriptor, self.type, els)

  def __eq__(self, o):
    if isinstance(o, Array):
      return self.descriptor == o.descriptor and \
          self.type == o.type and self.elements == o.elements
    else:
      return False


class Data(object):
  NULL = JData.DataType.NULL;
  BOOL = JData.DataType.BOOL;
  UBYTE = JData.DataType.UBYTE;
  BYTE = JData.DataType.BYTE;
  USHORT = JData.DataType.USHORT;
  SHORT = JData.DataType.SHORT;
  UINT = JData.DataType.UINT;
  INT = JData.DataType.INT;
  CHAR = JData.DataType.CHAR;
  ULONG = JData.DataType.ULONG;
  LONG = JData.DataType.LONG;
  TIMESTAMP = JData.DataType.TIMESTAMP;
  FLOAT = JData.DataType.FLOAT;
  DOUBLE = JData.DataType.DOUBLE;
  DECIMAL32 = JData.DataType.DECIMAL32;
  DECIMAL64 = JData.DataType.DECIMAL64;
  DECIMAL128 = JData.DataType.DECIMAL128;
  UUID = JData.DataType.UUID;
  BINARY = JData.DataType.BINARY;
  STRING = JData.DataType.STRING;
  SYMBOL = JData.DataType.SYMBOL;
  DESCRIBED = JData.DataType.DESCRIBED;
  ARRAY = JData.DataType.ARRAY;
  LIST = JData.DataType.LIST;
  MAP = JData.DataType.MAP;

  def __init__(self, capacity=16):
    self._data = Proton.data(capacity)

  def __del__(self):
    if hasattr(self, "_data"):
      pn_data_free(self._data)
      del self._data

  def clear(self):
    self._data.clear()

  def rewind(self):
    self._data.rewind()

  def next(self):
    return self._data.next()

  def prev(self):
    return self._data.prev()

  def enter(self):
    return self._data.enter()

  def exit(self):
    return self._data.exit()

  def lookup(self, name):
    return self._data.lookup(name)

  def narrow(self):
    self._data.narrow()

  def widen(self):
    self._data.widen()

  def type(self):
    return self._data.type()

  def encode(self):

    b = self._data.encode()
    return b.getArray().tostring()[b.getArrayOffset():b.getLength()]

  def decode(self, encoded):
    return self._data.decode(ByteBuffer.wrap(encoded))

  def put_list(self):
    self._data.putList()

  def put_map(self):
    self._data.putMap()

  def put_array(self, described, element_type):
    self._data.putArray(described, element_type)

  def put_described(self):
    self._data.putDescribed()

  def put_null(self):
    self._data.putNull()

  def put_bool(self, b):
    self._data.putBoolean(b)

  def put_ubyte(self, ub):
    self._data.putUnsignedByte(UnsignedByte.valueOf(ub))

  def put_byte(self, b):
    self._data.putByte(b)

  def put_ushort(self, us):
    self._data.putUnsignedShort(UnsignedShort.valueOf(us))

  def put_short(self, s):
    self._data.putShort(s)

  def put_uint(self, ui):
    self._data.putUnsignedInteger(UnsignedInteger.valueOf(ui))

  def put_int(self, i):
    self._data.putInt(i)

  def put_char(self, c):
    self._data.putChar(ord(c))

  def put_ulong(self, ul):
    self._data.putUnsignedLong(UnsignedLong.valueOf(ul))

  def put_long(self, l):
    self._data.putLong(l)

  def put_timestamp(self, t):
    self._data.putTimestamp(JDate(t))

  def put_float(self, f):
    self._data.putFloat(f)

  def put_double(self, d):
    self._data.putDouble(d)

  def put_decimal32(self, d):
    self._data.putDecimal32(Decimal32(d))

  def put_decimal64(self, d):
    self._data.putDecimal64(Decimal64(d))

  def put_decimal128(self, d):
    self._data.putDecimal128(Decimal128(d))

  def put_uuid(self, u):
    u = JUUID.fromString( str(u) )
    self._data.putUUID(u)

  def put_binary(self, b):
    self._data.putBinary(b)

  def put_string(self, s):
    self._data.putString(s)

  def put_symbol(self, s):
    self._data.putSymbol(Symbol.valueOf(s))

  def get_list(self):
    return self._data.getList()

  def get_map(self):
    return self._data.getMap()

  def get_array(self):
    count = self._data.getArray()
    described = self._data.isArrayDescribed()
    type = self._data.getArrayType()
    return count, described, type

  def is_described(self):
    return self._data.isDescribed()

  def is_null(self):
    return self._data.isNull()

  def get_bool(self):
    return self._data.getBoolean()

  def get_ubyte(self):
    return self._data.getUnsignedByte().shortValue()

  def get_byte(self):
    return self._data.getByte()

  def get_ushort(self):
    return self._data.getUnsignedShort().intValue()

  def get_short(self):
    return self._data.getShort()

  def get_int(self):
    return self._data.getInt()

  def get_uint(self):
    return self._data.getUnsignedInteger().longValue()

  def get_char(self):
    return char(unichr(self._data.getChar()))

  def get_ulong(self):
    return ulong(self._data.getUnsignedLong().longValue())

  def get_long(self):
    return self._data.getLong()

  def get_timestamp(self):
    return self._data.getTimestamp().getTime()

  def get_float(self):
    return self._data.getFloat()

  def get_double(self):
    return self._data.getDouble()

  def get_decimal32(self):
    return self._data.getDecimal32().getBits()

  def get_decimal64(self):
    return self._data.getDecimal64().getBits()

  def get_decimal128(self):
    return self._data.getDecimal128().asBytes().tostring()

  def get_uuid(self):
    return UUID(self._data.getUUID().toString() )

  def get_binary(self):
    b = self._data.getBinary()
    return b.getArray().tostring()[b.getArrayOffset():b.getArrayOffset()+b.getLength()]

  def get_string(self):
    return self._data.getString()

  def get_symbol(self):
    return symbol(self._data.getSymbol().toString())

  def put_dict(self, d):
    self.put_map()
    self.enter()
    try:
      for k, v in d.items():
        self.put_object(k)
        self.put_object(v)
    finally:
      self.exit()

  def get_dict(self):
    if self.enter():
      try:
        result = {}
        while self.next():
          k = self.get_object()
          if self.next():
            v = self.get_object()
          else:
            v = None
          result[k] = v
      finally:
        self.exit()
      return result

  def put_sequence(self, s):
    self.put_list()
    self.enter()
    try:
      for o in s:
        self.put_object(o)
    finally:
      self.exit()

  def get_sequence(self):
    if self.enter():
      try:
        result = []
        while self.next():
          result.append(self.get_object())
      finally:
        self.exit()
      return result

  def get_py_described(self):
    if self.enter():
      try:
        self.next()
        descriptor = self.get_object()
        self.next()
        value = self.get_object()
      finally:
        self.exit()
      return Described(descriptor, value)

  def put_py_described(self, d):
    self.put_described()
    self.enter()
    try:
      self.put_object(d.descriptor)
      self.put_object(d.value)
    finally:
      self.exit()

  def get_py_array(self):
    count, described, type = self.get_array()
    if self.enter():
      try:
        if described:
          self.next()
          descriptor = self.get_object()
        else:
          descriptor = UNDESCRIBED
        elements = []
        while self.next():
          elements.append(self.get_object())
      finally:
        self.exit()
      return Array(descriptor, type, *elements)

  def put_py_array(self, a):
    self.put_array(a.descriptor != UNDESCRIBED, a.type)
    self.enter()
    try:
      for e in a.elements:
        self.put_object(e)
    finally:
      self.exit()

  put_mappings = {
    None.__class__: lambda s, _: s.put_null(),
    bool: put_bool,
    dict: put_dict,
    list: put_sequence,
    tuple: put_sequence,
    unicode: put_string,
    bytes: put_binary,
    symbol: put_symbol,
    int: put_int,
    char: put_char,
    long: put_long,
    ulong: put_ulong,
    timestamp: put_timestamp,
    float: put_double,
    uuid.UUID: put_uuid,
    Described: put_py_described,
    Array: put_py_array
    }

  get_mappings = {
    NULL: lambda s: None,
    BOOL: get_bool,
    BYTE: get_byte,
    UBYTE: get_ubyte,
    SHORT: get_short,
    USHORT: get_ushort,
    INT: get_int,
    UINT: get_uint,
    CHAR: get_char,
    LONG: get_long,
    ULONG: get_ulong,
    TIMESTAMP: get_timestamp,
    FLOAT: get_float,
    DOUBLE: get_double,
    DECIMAL32: get_decimal32,
    DECIMAL64: get_decimal64,
    DECIMAL128: get_decimal128,
    UUID: get_uuid,
    BINARY: get_binary,
    STRING: get_string,
    SYMBOL: get_symbol,
    DESCRIBED: get_py_described,
    ARRAY: get_py_array,
    LIST: get_sequence,
    MAP: get_dict
    }

  def put_object(self, obj):
    putter = self.put_mappings[obj.__class__]
    putter(self, obj)


  def get_object(self):
    type = self.type()
    if type is None: return None
    getter = self.get_mappings.get(type)
    if getter:
      return getter(self)
    else:
      self.dump()
      return UnmappedType(str(type))

  def copy(self, src):
    self._data.copy(src._data)

  def format(self):
    return self._data.toString()

class Messenger(object):

  def __init__(self, name=None):
    if name:
      self.impl = Proton.messenger(name)
    else:
      self.impl = Proton.messenger()

  def route(self, pattern, address):
    self.impl.route(pattern, address)

  def rewrite(self, pattern, address):
    self.impl.rewrite(pattern, address)

  def start(self):
    self.impl.start()

  def stop(self):
    self.impl.stop()

  @property
  def stopped(self):
    return self.impl.stopped()

  def subscribe(self, source):
    self.impl.subscribe(source)

  def put(self, message):
    self.impl.put(message.impl)
    return self.impl.outgoingTracker()

  def send(self, n=-1):
    self.impl.send(n)

  def recv(self, n=-1):
    self.impl.recv(n)

  @property
  def receiving(self):
    return self.impl.receiving()

  def work(self, timeout=None):
    if timeout is None:
      t = -1
    else:
      t = long(1000*timeout)
    try:
        err = self.impl.work(t)
    except Timeout, e:
        return False
    return err

  def interrupt(self):
    self.impl.interrupt()

  def get(self, message=None):
    result = self.impl.get()
    if message and result:
      message.impl = result
    return self.impl.incomingTracker()

  @property
  def outgoing(self):
    return self.impl.outgoing()

  @property
  def incoming(self):
    return self.impl.incoming()

  def _get_timeout(self):
    t = self.impl.getTimeout()
    if t == -1:
      return None
    else:
      return float(t)/1000
  def _set_timeout(self, timeout):
    if timeout is None:
      t = -1
    else:
      t = long(1000*timeout)
    self.impl.setTimeout(t)
  timeout = property(_get_timeout, _set_timeout)

  def _is_blocking(self):
    return self.impl.isBlocking()

  def _set_blocking(self, b):
    self.impl.setBlocking(b)

  blocking = property(_is_blocking, _set_blocking)
  def accept(self, tracker=None):
    if tracker is None:
      tracker = self.impl.incomingTracker()
      flags = self.impl.CUMULATIVE
    else:
      flags = 0
    self.impl.accept(tracker, flags)

  def reject(self, tracker=None):
    if tracker is None:
      tracker = self.impl.incomingTracker()
      flags = self.impl.CUMULATIVE
    else:
      flags = 0
    self.impl.reject(tracker, flags)

  def settle(self, tracker=None):
    if tracker is None:
      tracker = self.impl.outgoingTracker()
      flags = self.impl.CUMULATIVE
    else:
      flags = 0
    self.impl.settle(tracker, flags)

  def status(self, tracker):
    return STATUSES[self.impl.getStatus(tracker)]

  def _get_incoming_window(self):
    return self.impl.getIncomingWindow()
  def _set_incoming_window(self, window):
    self.impl.setIncomingWindow(window)
  incoming_window = property(_get_incoming_window, _set_incoming_window)

  def _get_outgoing_window(self):
    return self.impl.getOutgoingWindow()
  def _set_outgoing_window(self, window):
    self.impl.setOutgoingWindow(window)
  outgoing_window = property(_get_outgoing_window, _set_outgoing_window)

  def _get_certificate(self):
    raise Skipped()
  def _set_certificate(self, xxx):
    raise Skipped()
  certificate = property(_get_certificate, _set_certificate)

  def buffered(self, tracker):
    raise Skipped()


class Message(object):

  AMQP = MessageFormat.AMQP
  TEXT = MessageFormat.TEXT
  DATA = MessageFormat.DATA
  JSON = MessageFormat.JSON

  DEFAULT_PRIORITY = JMessage.DEFAULT_PRIORITY

  def __init__(self):
    self.impl = Proton.message()

  def clear(self):
    self.impl.clear()

  def save(self):
    saved = self.impl.save()
    if saved is None:
      saved = ""
    elif not isinstance(saved, unicode):
      saved = saved.tostring()
    return saved

  def load(self, data):
    self.impl.load(data)

  def encode(self):
    size = 1024
    output = zeros(size, "b")
    while True:
      n = self.impl.encode(output, 0, size)
      # XXX: need to check for overflow
      if n > 0:
        return output.tostring()[:n]
      else:
        raise Exception(n)

  def decode(self, data):
    self.impl.decode(data,0,len(data))

  def _get_id(self):
    id = self.impl.getMessageId()
    if isinstance(id, JUUID):
      id = UUID( id.toString() )
    return id
  def _set_id(self, value):
    if isinstance(value, UUID):
      value = JUUID.fromString( str(value) )
    return self.impl.setMessageId(value)
  id = property(_get_id, _set_id)

  def _get_correlation_id(self):
    id = self.impl.getCorrelationId()
    if isinstance(id, JUUID):
      id = UUID( id.toString() )
    return id
  def _set_correlation_id(self, value):
    if isinstance(value, UUID):
      value = JUUID.fromString( str(value) )
    return self.impl.setCorrelationId(value)
  correlation_id = property(_get_correlation_id, _set_correlation_id)

  def _get_ttl(self):
    return self.impl.getTtl()
  def _set_ttl(self, ttl):
    self.impl.setTtl(ttl)
  ttl = property(_get_ttl, _set_ttl)

  def _get_priority(self):
    return self.impl.getPriority()
  def _set_priority(self, priority):
    self.impl.setPriority(priority)
  priority = property(_get_priority, _set_priority)

  def _get_address(self):
    return self.impl.getAddress()
  def _set_address(self, address):
    self.impl.setAddress(address)
  address = property(_get_address, _set_address)

  def _get_subject(self):
    return self.impl.getSubject()
  def _set_subject(self, subject):
    self.impl.setSubject(subject)
  subject = property(_get_subject, _set_subject)

  def _get_user_id(self):
    u = self.impl.getUserId()
    if u is None: return ""
    else: return u.tostring()
  def _set_user_id(self, user_id):
    self.impl.setUserId(user_id)
  user_id = property(_get_user_id, _set_user_id)

  def _get_reply_to(self):
    return self.impl.getReplyTo()
  def _set_reply_to(self, reply_to):
    self.impl.setReplyTo(reply_to)
  reply_to = property(_get_reply_to, _set_reply_to)

  def _get_reply_to_group_id(self):
    return self.impl.getReplyToGroupId()
  def _set_reply_to_group_id(self, reply_to_group_id):
    self.impl.setReplyToGroupId(reply_to_group_id)
  reply_to_group_id = property(_get_reply_to_group_id, _set_reply_to_group_id)

  def _get_group_id(self):
    return self.impl.getGroupId()
  def _set_group_id(self, group_id):
    self.impl.setGroupId(group_id)
  group_id = property(_get_group_id, _set_group_id)

  def _get_group_sequence(self):
    return self.impl.getGroupSequence()
  def _set_group_sequence(self, group_sequence):
    self.impl.setGroupSequence(group_sequence)
  group_sequence = property(_get_group_sequence, _set_group_sequence)

  def _is_first_acquirer(self):
    return self.impl.isFirstAcquirer()
  def _set_first_acquirer(self, b):
    self.impl.setFirstAcquirer(b)
  first_acquirer = property(_is_first_acquirer, _set_first_acquirer)

  def _get_expiry_time(self):
    return self.impl.getExpiryTime()
  def _set_expiry_time(self, expiry_time):
    self.impl.setExpiryTime(expiry_time)
  expiry_time = property(_get_expiry_time, _set_expiry_time)

  def _is_durable(self):
    return self.impl.isDurable()
  def _set_durable(self, durable):
    self.impl.setDurable(durable)
  durable = property(_is_durable, _set_durable)

  def _get_delivery_count(self):
    return self.impl.getDeliveryCount()
  def _set_delivery_count(self, delivery_count):
    self.impl.setDeliveryCount(delivery_count)
  delivery_count = property(_get_delivery_count, _set_delivery_count)

  def _get_creation_time(self):
    return self.impl.getCreationTime()
  def _set_creation_time(self, creation_time):
    self.impl.setCreationTime(creation_time)
  creation_time = property(_get_creation_time, _set_creation_time)

  def _get_content_type(self):
    return self.impl.getContentType()
  def _set_content_type(self, content_type):
    self.impl.setContentType(content_type)
  content_type = property(_get_content_type, _set_content_type)

  def _get_content_encoding(self):
    return self.impl.getContentEncoding()
  def _set_content_encoding(self, content_encoding):
    self.impl.setContentEncoding(content_encoding)
  content_encoding = property(_get_content_encoding, _set_content_encoding)

  def _get_format(self):
    return self.impl.getFormat()
  def _set_format(self, format):
    self.impl.setMessageFormat(format)
  format = property(_get_format, _set_format)

  def _get_body(self):
    body = self.impl.getBody()
    if isinstance(body, AmqpValue):
      return body.getValue()
    else:
      return body
  def _set_body(self, body):
    self.impl.setBody(AmqpValue(body))
  body = property(_get_body, _set_body)


class SASL(object):

  OK = Sasl.PN_SASL_OK
  AUTH = Sasl.PN_SASL_AUTH

  def __new__(cls, transport):
    """Enforce a singleton SASL object per Transport"""
    if not transport._sasl:
      obj = super(SASL, cls).__new__(cls)
      obj._sasl = transport.impl.sasl()
      transport._sasl = obj
    return transport._sasl

  def mechanisms(self, mechanisms):
    self._sasl.setMechanisms(mechanisms.split())

  def client(self):
    self._sasl.client()

  def server(self):
    self._sasl.server()

  def send(self, data):
    self._sasl.send(data, 0, len(data))

  def recv(self):
    size = 4096
    output = zeros(size, "b")
    n = self._sasl.recv(output, 0, size)
    if n >= 0:
      return output.tostring()[:n]
    elif n == JTransport.END_OF_STREAM:
      return None
    else:
      raise Exception(n)

  def _get_outcome(self):
    value = self._sasl.getOutcome()
    if value == Sasl.PN_SASL_NONE:
      return None
    else:
      return value
  def _set_outcome(self, outcome):
    self.impl.setOutcome(outcome)

  outcome = property(_get_outcome, _set_outcome)

  def done(self, outcome):
    self._sasl.done(outcome)

  def plain(self, user, password):
    self._sasl.plain(user,password)

class SSLException(Exception):
  pass

class SSLUnavailable(SSLException):
  pass

class SSLDomain(object):

  MODE_SERVER = JSslDomain.Mode.SERVER
  MODE_CLIENT = JSslDomain.Mode.CLIENT
  VERIFY_PEER = JSslDomain.VerifyMode.VERIFY_PEER
  VERIFY_PEER_NAME = JSslDomain.VerifyMode.VERIFY_PEER_NAME
  ANONYMOUS_PEER = JSslDomain.VerifyMode.ANONYMOUS_PEER

  def __init__(self, mode):
    try:
      self._domain = Proton.sslDomain()
    except NoClassDefFoundError, e:
      raise SSLUnavailable()
    self._domain.init(mode)

  def set_credentials(self, cert_file, key_file, password):
    self._domain.setCredentials(cert_file, key_file, password)

  def set_trusted_ca_db(self, certificate_db):
    self._domain.setTrustedCaDb(certificate_db)

  def set_peer_authentication(self, verify_mode, trusted_CAs=None):
    # TODO the method calls (setTrustedCaDb/setPeerAuthentication) have to occur in
    # that order otherwise tests fail with proton-jni.  It is not clear yet why.
    if trusted_CAs is not None:
      self._domain.setTrustedCaDb(trusted_CAs)
    self._domain.setPeerAuthentication(verify_mode)

  def allow_unsecured_client(self, allow_unsecured = True):
    self._domain.allowUnsecuredClient(allow_unsecured)

class SSLSessionDetails(object):

  def __init__(self, session_id):
    self._session_details = Proton.sslPeerDetails(session_id, 1)

class SSL(object):

  def __new__(cls, transport, domain, session_details=None):
    """Enforce a singleton SSL object per Transport"""
    if transport._ssl:
      # unfortunately, we've combined the allocation and the configuration in a
      # single step.  So catch any attempt by the application to provide what
      # may be a different configuration than the original (hack)
      ssl = transport._ssl
      if (domain and (ssl._domain is not domain) or
          session_details and (ssl._session_details is not session_details)):
        raise SSLException("Cannot re-configure existing SSL object!")
    else:
      obj = super(SSL, cls).__new__(cls)
      obj._domain = domain
      obj._session_details = session_details

      internal_session_details = None
      if session_details:
        internal_session_details = session_details._session_details

      obj._ssl = transport.impl.ssl(domain._domain, internal_session_details)
      transport._ssl = obj
    return transport._ssl

  def __init__(self, transport, domain, session_details=None):

    internal_session_details = None
    if session_details:
      internal_session_details = session_details._session_details

    self._ssl = transport.impl.ssl(domain._domain, internal_session_details)
    self._session_details = session_details

  def get_session_details(self):
    return self._session_details

  def cipher_name(self):
    return self._ssl.getCipherName()

  def protocol_name(self):
    return self._ssl.getProtocolName()

  def _set_peer_hostname(self, hostname):
    self._ssl.setPeerHostname(hostname)

  def _get_peer_hostname(self):
    return self._ssl.getPeerHostname()

  peer_hostname = property(_get_peer_hostname, _set_peer_hostname)


class Driver(object):
  """ Proton-c platform abstraction - not needed."""
  def __init__(self, *args, **kwargs):
    raise ProtonUnsupportedOperationException("Driver")
class Connector(object):
  """ Proton-c platform abstraction - not needed."""
  def __init__(self, *args, **kwargs):
    raise ProtonUnsupportedOperationException("Connector")
class Listener(object):
  """ Proton-c platform abstraction - not needed."""
  def __init__(self, *args, **kwargs):
    raise ProtonUnsupportedOperationException("Listener")

def convertToPyArray(t,a,f):
    if a == None or len(a) == 0:
        return None

    return Array(UNDESCRIBED, t, *map(f,a))


arrayElementMappings = {
    JData.DataType.SYMBOL: lambda s: Symbol.valueOf(s)
    }

arrayTypeMappings = {
    JData.DataType.SYMBOL: Symbol
    }

conversions_J2PY = {
  dict: lambda d: dict([(J2PY(k), J2PY(v)) for k, v in d.items()]),
  HashMap: lambda m: dict([(J2PY(e.getKey()), J2PY(e.getValue())) for e in m.entrySet()]),
  list: lambda l: [J2PY(x) for x in l],
  Symbol: lambda s: symbol(s.toString()),
  UnsignedInteger: lambda n: n.longValue(),
  UnsignedLong: lambda n: n.longValue()
  }

conversions_PY2J = {
  dict: lambda d: dict([(PY2J(k), PY2J(v)) for k, v in d.items()]),
  list: lambda l: [PY2J(x) for x in l],
  symbol: lambda s: Symbol.valueOf(s),
  Array: lambda a: array(map(arrayElementMappings[a.type], a.elements),
                         arrayTypeMappings[a.type])
  }

def identity(x): return x

def J2PY(obj):
  result = conversions_J2PY.get(type(obj), identity)(obj)
  return result

def PY2J(obj):
  result = conversions_PY2J.get(type(obj), identity)(obj)
  return result

__all__ = [
           "ACCEPTED",
           "Array",
           "API_LANGUAGE",
           "IMPLEMENTATION_LANGUAGE",
           "MANUAL",
           "PENDING",
           "REJECTED",
           "RELEASED",
           "SETTLED",
           "char",
           "Condition",
           "Connection",
           "Connector",
           "Data",
           "Delivery",
           "Disposition",
           "Described",
           "Driver",
           "Endpoint",
           "Link",
           "Listener",
           "Message",
           "MessageException",
           "Messenger",
           "MessengerException",
           "ProtonException",
           "Receiver",
           "SASL",
           "Sender",
           "Session",
           "SSL",
           "SSLDomain",
           "SSLException",
           "SSLSessionDetails",
           "SSLUnavailable",
           "symbol",
           "timestamp",
           "Terminus",
           "Timeout",
           "Interrupt",
           "Transport",
           "TransportException",
           "ulong",
           "UNDESCRIBED"]
