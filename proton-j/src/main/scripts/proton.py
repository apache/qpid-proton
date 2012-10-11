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

from org.apache.qpid.proton.engine import EndpointState, Accepted, TransportException
from org.apache.qpid.proton.engine.impl import ConnectionImpl, SessionImpl, \
    SenderImpl, ReceiverImpl, TransportImpl
from org.apache.qpid.proton.message import Message as MessageImpl, \
    MessageFormat
from jarray import zeros
from java.util import EnumSet

class Skipped(Exception):
  skipped = True

PN_SESSION_WINDOW = TransportImpl.SESSION_WINDOW

class Endpoint(object):

  LOCAL_UNINIT = 1
  LOCAL_ACTIVE = 2
  LOCAL_CLOSED = 4
  REMOTE_UNINIT = 8
  REMOTE_ACTIVE = 16
  REMOTE_CLOSED = 32

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
    self.impl.close()

def wrap_connection(impl):
  if impl: return Connection(_impl = impl)

class Connection(Endpoint):

  def __init__(self, _impl=None):
    self.impl = _impl or ConnectionImpl()

  @property
  def writable(self):
    raise Skipped()

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



def wrap_session(impl):
  # XXX
  if impl: return Session(impl)

class Session(Endpoint):

  def __init__(self, impl):
    self.impl = impl

  @property
  def connection(self):
    return wrap_connection(self.impl.getConnection())

  def sender(self, name):
    return wrap_link(self.impl.sender(name))

  def receiver(self, name):
    return wrap_link(self.impl.receiver(name))

def wrap_link(impl):
  if impl is None: return None
  elif isinstance(impl, SenderImpl):
    return Sender(impl)
  elif isinstance(impl, ReceiverImpl):
    return Receiver(impl)
  else:
    raise Exception("unknown type")

class Link(Endpoint):

  def __init__(self, impl):
    self.impl = impl

  @property
  def source(self):
    return Terminus(self.impl, "LocalSource")

  @property
  def target(self):
    return Terminus(self.impl, "LocalTarget")

  @property
  def remote_source(self):
    return Terminus(self.impl, "RemoteSource")
  @property
  def remote_target(self):
    return Terminus(self.impl, "RemoteTarget")

  @property
  def session(self):
    return wrap_session(self.impl.getSession())

  def delivery(self, tag):
    return wrap_delivery(self.impl.delivery(tag, 0, len(tag)))

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
    raise Skipped()

  @property
  def queued(self):
    return self.impl.getQueued()

  def next(self, mask):
    return self.impl.next(*self._enums(mask))

class DataDummy:

  def format(self):
    pass

  def put_array(self, *args, **kwargs):
    raise Skipped()

class Terminus(object):

  UNSPECIFIED = None

  def __init__(self, impl, prefix):
    self.impl = impl
    self.prefix = prefix
    self.type = None
    self.timeout = None
    self.durability = None
    self.expiry_policy = None
    self.dynamic = None
    self.properties = DataDummy()
    self.outcomes = DataDummy()
    self.filter = DataDummy()
    self.capabilities = DataDummy()

  def _get_address(self):
    return getattr(self.impl, "get%sAddress" % self.prefix)()
  def _set_address(self, address):
    getattr(self.impl, "set%sAddress" % self.prefix)(address)
  address = property(_get_address, _set_address)

  def copy(self, src):
    self.address = src.address

class Sender(Link):

  def offered(self, n):
    raise Skipped()

  def send(self, bytes):
    return self.impl.send(bytes, 0, len(bytes))

  def drained(self):
    self.impl.drained()

class Receiver(Link):

  def flow(self, n):
    self.impl.flow(n)

  def drain(self, n):
    self.impl.drain(n)

  def recv(self, size):
    output = zeros(size, "b")
    n = self.impl.recv(output, 0, size)
    if n >= 0:
      return output.tostring()[:n]
    elif n == TransportImpl.END_OF_STREAM:
      return None
    else:
      raise Exception(n)

def wrap_delivery(impl):
  if impl: return Delivery(impl)

class Delivery(object):

  RECEIVED = 1
  ACCEPTED = 2
  REJECTED = 3
  RELEASED = 4
  MODIFIED = 5

  def __init__(self, impl):
    self.impl = impl

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
    if disp == self.ACCEPTED:
      self.impl.disposition(Accepted.getInstance())
    else:
      raise Exception("xxx: %s" % disp)

  @property
  def remote_state(self):
    rd = self.impl.getRemoteState()
    if(rd == Accepted.getInstance()):
      return self.ACCEPTED
    else:
      raise Exception("xxx: %s" % rd)

  @property
  def local_state(self):
    ld = self.impl.getLocalState()
    if(ld == Accepted.getInstance()):
      return self.ACCEPTED
    else:
      raise Exception("xxx: %s" % ld)

  def settle(self):
    self.impl.settle()

  @property
  def settled(self):
    return self.impl.remotelySettled()

  @property
  def work_next(self):
    return wrap_delivery(self.impl.getWorkNext())

class Transport(object):

  TRACE_OFF = 0
  TRACE_RAW = 1
  TRACE_FRM = 2
  TRACE_DRV = 4

  def __init__(self):
    self.impl = TransportImpl()

  def trace(self, mask):
    self.impl.trace(mask)

  def bind(self, connection):
    self.impl.bind(connection.impl)

  def output(self, size):
    output = zeros(size, "b")
    n = self.impl.output(output, 0, size)
    if n >= 0:
      return output.tostring()[:n]
    elif n == TransportImpl.END_OF_STREAM:
      return None
    else:
      raise Exception("XXX: %s" % n)

  def input(self, bytes):
    return self.impl.input(bytes, 0, len(bytes))

class Data(object):

  SYMBOL = None

  def __init__(self, *args, **kwargs):
    raise Skipped()

class Messenger(object):

  def __init__(self, *args, **kwargs):
    raise Skipped()

class Message(object):

  AMQP = MessageFormat.AMQP
  TEXT = MessageFormat.TEXT
  DATA = MessageFormat.DATA
  JSON = MessageFormat.JSON

  DEFAULT_PRIORITY = MessageImpl.DEFAULT_PRIORITY

  def __init__(self):
    self.impl = MessageImpl()

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

class SASL(object):

  def __init__(self, *args, **kwargs):
    raise Skipped()

class SSL(object):

  def __init__(self, *args, **kwargs):
    raise Skipped()


__all__ = ["Messenger", "Message", "ProtonException", "MessengerException",
           "MessageException", "Timeout", "Data", "Endpoint", "Connection",
           "Session", "Link", "Terminus", "Sender", "Receiver", "Delivery",
           "Transport", "TransportException", "SASL", "SSL",
           "PN_SESSION_WINDOW"]
