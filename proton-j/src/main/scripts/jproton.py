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

from org.apache.qpid.proton.engine import *
from org.apache.qpid.proton.message import *
from jarray import zeros
from java.util import EnumSet

class Skipped(Exception):
  skipped = True

PN_SESSION_WINDOW = impl.TransportImpl.SESSION_WINDOW
PN_EOS = Transport.END_OF_STREAM
PN_ERR = -2
PN_OVERFLOW = -3
PN_UNDERFLOW = -4
PN_STATE_ERR = -5
PN_ARG_ERR = -6
PN_TIMEOUT = -7

PN_LOCAL_UNINIT = 1
PN_LOCAL_ACTIVE = 2
PN_LOCAL_CLOSED = 4
PN_REMOTE_UNINIT = 8
PN_REMOTE_ACTIVE = 16
PN_REMOTE_CLOSED = 32

PN_RECEIVED = 1
PN_ACCEPTED = 2
PN_REJECTED = 3
PN_RELEASED = 4
PN_MODIFIED = 5

PN_DEFAULT_PRIORITY = Message.DEFAULT_PRIORITY

PN_AMQP = MessageFormat.AMQP
PN_TEXT = MessageFormat.TEXT
PN_DATA = MessageFormat.DATA
PN_JSON = MessageFormat.JSON

PN_NULL = 1
PN_BOOL = 2
PN_UBYTE = 3
PN_BYTE = 4
PN_USHORT = 5
PN_SHORT = 6
PN_UINT = 7
PN_INT = 8
PN_ULONG = 9
PN_LONG = 10
PN_FLOAT = 11
PN_DOUBLE = 12
PN_BINARY = 13
PN_STRING = 14
PN_SYMBOL = 15
PN_DESCRIPTOR = 16
PN_ARRAY = 17
PN_LIST = 18
PN_MAP = 19

def enums(mask):
  local = []
  if (PN_LOCAL_UNINIT | mask):
    local.append(EndpointState.UNINITIALIZED)
  if (PN_LOCAL_ACTIVE | mask):
    local.append(EndpointState.ACTIVE)
  if (PN_LOCAL_CLOSED | mask):
    local.append(EndpointState.CLOSED)

  remote = []
  if (PN_REMOTE_UNINIT | mask):
    remote.append(EndpointState.UNINITIALIZED)
  if (PN_REMOTE_ACTIVE | mask):
    remote.append(EndpointState.ACTIVE)
  if (PN_REMOTE_CLOSED | mask):
    remote.append(EndpointState.CLOSED)

  return EnumSet.of(*local), EnumSet.of(*remote)

def state(endpoint):
  local = endpoint.getLocalState()
  remote = endpoint.getRemoteState()

  result = 0

  if (local == EndpointState.UNINITIALIZED):
    result = result | PN_LOCAL_UNINIT
  elif (local == EndpointState.ACTIVE):
    result = result | PN_LOCAL_ACTIVE
  elif (local == EndpointState.CLOSED):
    result = result | PN_LOCAL_CLOSED

  if (remote == EndpointState.UNINITIALIZED):
    result = result | PN_REMOTE_UNINIT
  elif (remote == EndpointState.ACTIVE):
    result = result | PN_REMOTE_ACTIVE
  elif (remote == EndpointState.CLOSED):
    result = result | PN_REMOTE_CLOSED

  return result


def pn_connection():
  return impl.ConnectionImpl()

def pn_connection_free(c):
  pass

def pn_transport_free(c):
  pass

def pn_connection_state(c):
  return state(c)

def pn_connection_open(c):
  return c.open()

def pn_connection_close(c):
  return c.close()

def pn_session(c):
  return c.session()

def pn_session_free(s):
  pass

def pn_session_state(s):
  return state(s)

def pn_session_open(s):
  return s.open()

def pn_session_close(s):
  return s.close()

def pn_transport():
  return impl.TransportImpl()

def pn_transport_bind(t,c):
  return t.bind(c)

def pn_trace(t, lvl):
  t.setLogLevel(lvl)
  pass

def pn_output(t, size):
  output = zeros(size, "b")
  n = t.output(output, 0, size)
  result = ""
  if n > 0:
    result = output.tostring()[:n]
  return [n, result]

def pn_input(t, inp):
  return t.input(inp, 0, len(inp))

def pn_session_head(c, mask):
  local, remote = enums(mask)
  return c.sessionHead(local, remote)

def pn_sender(ssn, name):
  return ssn.sender(name)

def pn_receiver(ssn, name):
  return ssn.receiver(name)

def pn_link_free(lnk):
  pass

def pn_link_state(lnk):
  return state(lnk)

def pn_link_open(lnk):
  return lnk.open()

def pn_link_close(lnk):
  return lnk.close()

def pn_work_head(c):
  return c.getWorkHead()

def pn_work_next(d):
  return d.getWorkNext()

def pn_link_head(c, m):
  local, remote = enums(m)
  return c.linkHead(local, remote)


def pn_link_next(l, m):
  local, remote = enums(m)
  return l.next(local, remote)

def pn_flow(rcv, n):
  return rcv.flow(n)

def pn_send(snd, msg):
  return snd.send(msg, 0, len(msg))

def pn_delivery(lnk, tag):
  return lnk.delivery(tag, 0, len(tag))

def pn_delivery_tag(d):
  return d.getTag().tostring()

def pn_writable(d):
  return d.isWritable()

def pn_readable(d):
  return d.isReadable()

def pn_updated(d):
  return d.isUpdated()

def pn_advance(l):
  return l.advance()

def pn_current(l):
  return l.current()

def pn_recv(l, size):
  output = zeros(size, "b")
  n = l.recv(output, 0, size)
  result = ""
  if n > 0:
    result = output.tostring()[:n]
  return [n, result]

def pn_disposition(d, p):
  if p == PN_ACCEPTED:
    d.disposition(Accepted.getInstance())


def pn_remote_settled(d):
  return d.remotelySettled()

def pn_remote_disp(d):
  if(d.getRemoteState() == Accepted.getInstance()):
    return PN_ACCEPTED

def pn_remote_disposition(d):
  if(d.getRemoteState() == Accepted.getInstance()):
    return PN_ACCEPTED

def pn_local_disp(d):
  if(d.getLocalState() == Accepted.getInstance()):
    return PN_ACCEPTED


def pn_local_disposition(d):
  if(d.getLocalState() == Accepted.getInstance()):
    return PN_ACCEPTED

def pn_settle(d):
  d.settle()


def pn_get_connection(s):
  return s.getConnection()

def pn_get_session(l):
  return l.getSession()

def pn_credit(l):
  return l.getCredit()

def pn_queued(l):
  return l.getQueued()

def pn_unsettled(l):
  return l.getUnsettled()

def pn_drain(l, c):
  l.drain(c)

def pn_drained(l):
  l.drained()

def pn_message():
  return Message()

def pn_message_is_durable(m):
  return m.isDurable()

def pn_message_set_durable(m,d):
  m.setDurable(d)
  return 0

def pn_message_get_priority(m):
  return m.getPriority()

def pn_message_set_priority(m,p):
  m.setPriority(p)
  return 0

def pn_message_get_ttl(m):
  return m.getTtl()

def pn_message_set_ttl(m, t):
  m.setTtl(t)
  return 0

def pn_message_is_first_acquirer(m):
  return m.isFirstAcquirer()

def pn_message_set_first_acquirer(m, b):
  m.setFirstAcquirer(b)
  return 0

def pn_message_get_delivery_count(m):
  return m.getDeliveryCount()

def pn_message_set_delivery_count(m,c):
  m.setDeliveryCount(c)
  return 0

def pn_message_get_id(m):
  return m.getId()

def pn_message_set_id(m, i):
  m.setId(i)
  return 0

def pn_message_get_user_id(m):
  u = m.getUserId()
  if u is None:
      return ""
  else:
      return u.tostring()

def pn_message_set_user_id(m, u):
  m.setUserId(u)
  return 0

def pn_message_load(m, d):
  m.load(d)
  return 0

def pn_message_save(m, s):
  saved = m.save()
  if saved is None:
    saved = ""
  elif not isinstance(saved, unicode):
    saved = saved.tostring()
  return 0, saved


def pn_message_get_address(m):
  return m.getAddress()

def pn_message_set_address(m, a):
  m.setAddress(a)
  return 0

def pn_message_get_subject(m):
  return m.getSubject()

def pn_message_set_subject(m,d):
  m.setSubject(d)
  return 0

def pn_message_get_reply_to(m):
  return m.getReplyTo()

def pn_message_set_reply_to(m,d):
  m.setReplyTo(d)
  return 0

def pn_message_get_correlation_id(m):
  return m.getCorrelationId()

def pn_message_set_correlation_id(m,d):
  m.setCorrelationId(d)
  return 0

def pn_message_get_content_type(m):
  return m.getContentType()

def pn_message_set_content_type(m,d):
  m.setContentType(d)
  return 0

def pn_message_get_content_encoding(m):
  return m.getContentEncoding()

def pn_message_set_content_encoding(m,d):
  m.setContentEncoding(d)
  return 0

def pn_message_get_expiry_time(m):
  return m.getExpiryTime()

def pn_message_set_expiry_time(m,d):
  m.setExpiryTime(d)
  return 0

def pn_message_get_creation_time(m):
  return m.getCreationTime()

def pn_message_set_creation_time(m,d):
  m.setCreationTime(d)
  return 0

def pn_message_get_group_id(m):
  return m.getGroupId()

def pn_message_set_group_id(m,d):
  m.setGroupId(d)
  return 0

def pn_message_get_group_sequence(m):
  return m.getGroupSequence()

def pn_message_set_group_sequence(m,d):
  m.setGroupSequence(d)
  return 0

def pn_message_get_reply_to_group_id(m):
  return m.getReplyToGroupId()

def pn_message_set_reply_to_group_id(m,d):
  m.setReplyToGroupId(d)
  return 0

def pn_message_free(m):
  return

def pn_message_encode(m,size):
    output = zeros(size, "b")
    n = m.encode(output, 0, size)
    result = ""
    if n > 0:
      result = output.tostring()[:n]
    return [0, result]

def pn_message_decode(m,data,size):
    m.decode(data,0,size)
    return 0

def pn_message_set_format(m, f):
    m.setMessageFormat(f)

def pn_message_get_format(m):
    return m.getMessageFormat()

def pn_message_clear(m):
    m.clear()

def pn_message_error(m):
    return m.getError().ordinal()

def pn_data(*args, **kwargs):
  raise Skipped()

def pn_messenger(*args, **kwargs):
  raise Skipped()
