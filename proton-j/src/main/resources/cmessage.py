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
from org.apache.qpid.proton.amqp.messaging import AmqpValue, AmqpSequence, \
  Data as DataSection, ApplicationProperties, MessageAnnotations, DeliveryAnnotations

from ccodec import *
from cerror import *
from org.apache.qpid.proton.amqp import Binary

# from proton/message.h
PN_DATA = 0
PN_TEXT = 1
PN_AMQP = 2
PN_JSON = 3

PN_DEFAULT_PRIORITY = 4

class pn_message_wrapper:

  def __init__(self):
    self.inferred = False
    self.impl = Proton.message()
    self.id = pn_data(0)
    self.correlation_id = pn_data(0)
    self.instructions = pn_data(0)
    self.annotations = pn_data(0)
    self.properties = pn_data(0)
    self.body = pn_data(0)

  def decode(self, impl):
    self.impl = impl
    self.post_decode()

  def post_decode(self):
    obj2dat(self.impl.getMessageId(), self.id)
    self.id.next()
    obj2dat(self.impl.getCorrelationId(), self.correlation_id)
    self.correlation_id.next()
    def peel(x):
       if x is not None:
         return x.getValue()
       return None
    obj2dat(peel(self.impl.getDeliveryAnnotations()), self.instructions)
    obj2dat(peel(self.impl.getMessageAnnotations()), self.annotations)
    obj2dat(peel(self.impl.getApplicationProperties()), self.properties)
    bod = self.impl.getBody()
    if bod is not None: bod = bod.getValue()
    obj2dat(bod, self.body)

  def pre_encode(self):
    self.impl.setMessageId(dat2obj(self.id))
    self.impl.setCorrelationId(dat2obj(self.correlation_id))
    def wrap(x, wrapper):
      if x is not None:
        return wrapper(x)
      return None
    self.impl.setDeliveryAnnotations(wrap(dat2obj(self.instructions), DeliveryAnnotations))
    self.impl.setMessageAnnotations(wrap(dat2obj(self.annotations), MessageAnnotations))
    self.impl.setApplicationProperties(wrap(dat2obj(self.properties), ApplicationProperties))
    bod = dat2obj(self.body)
    if self.inferred:
      if isinstance(bod, bytes):
        bod = DataSection(Binary(array(bod, 'b')))
      elif isinstance(bod, list):
        bod = AmqpSequence(bod)
      else:
        bod = AmqpValue(bod)
    else:
      bod = AmqpValue(bod)
    self.impl.setBody(bod)

  def __repr__(self):
    return self.impl.toString()

def pn_message():
  return pn_message_wrapper()

def pn_message_id(msg):
  return msg.id

def pn_message_correlation_id(msg):
  return msg.correlation_id

def pn_message_get_address(msg):
  return msg.impl.getAddress()

def pn_message_set_address(msg, address):
  msg.impl.setAddress(address)
  return 0

def pn_message_get_reply_to(msg):
  return msg.impl.getReplyTo()

def pn_message_set_reply_to(msg, address):
  msg.impl.setReplyTo(address)
  return 0

def pn_message_get_reply_to_group_id(msg):
  return msg.impl.getReplyToGroupId()

def pn_message_set_reply_to_group_id(msg, id):
  msg.impl.setReplyToGroupId(id)
  return 0

def pn_message_get_group_sequence(msg):
  return msg.impl.getGroupSequence()

def pn_message_set_group_sequence(msg, seq):
  msg.impl.setGroupSequence(seq)
  return 0

def pn_message_get_group_id(msg):
  return msg.impl.getGroupId()

def pn_message_set_group_id(msg, id):
  msg.impl.setGroupId(id)
  return 0

def pn_message_is_first_acquirer(msg):
  return msg.impl.isFirstAcquirer()

def pn_message_set_first_acquirer(msg, b):
  msg.impl.setFirstAcquirer(b)
  return 0

def pn_message_is_durable(msg):
  return msg.impl.isDurable()

def pn_message_set_durable(msg, b):
  msg.impl.setDurable(b)
  return 0

def pn_message_get_delivery_count(msg):
  return msg.impl.getDeliveryCount()

def pn_message_set_delivery_count(msg, c):
  msg.impl.setDeliveryCount(c)
  return 0

def pn_message_get_creation_time(msg):
  return msg.impl.getCreationTime()

def pn_message_set_creation_time(msg, t):
  msg.impl.setCreationTime(t)
  return 0

def pn_message_get_expiry_time(msg):
  return msg.impl.getExpiryTime()

def pn_message_set_expiry_time(msg, t):
  msg.impl.setExpiryTime(t)
  return 0

def pn_message_get_content_type(msg):
  return msg.impl.getContentType()

def pn_message_set_content_type(msg, ct):
  msg.impl.setContentType(ct)
  return 0

def pn_message_get_content_encoding(msg):
  return msg.impl.getContentEncoding()

def pn_message_set_content_encoding(msg, ct):
  msg.impl.setContentEncoding(ct)
  return 0

def pn_message_get_subject(msg):
  return msg.impl.getSubject()

def pn_message_set_subject(msg, value):
  msg.impl.setSubject(value)
  return 0

def pn_message_get_priority(msg):
  return msg.impl.getPriority()

def pn_message_set_priority(msg, p):
  msg.impl.setPriority(p)
  return 0

def pn_message_get_ttl(msg):
  return msg.impl.getTtl()

def pn_message_set_ttl(msg, ttl):
  msg.impl.setTtl(ttl)
  return 0

def pn_message_get_user_id(msg):
  uid = msg.impl.getUserId()
  if uid is None:
    return ""
  else:
    return uid.tostring()

def pn_message_set_user_id(msg, uid):
  msg.impl.setUserId(uid)
  return 0

def pn_message_instructions(msg):
  return msg.instructions

def pn_message_annotations(msg):
  return msg.annotations

def pn_message_properties(msg):
  return msg.properties

def pn_message_body(msg):
  return msg.body

def pn_message_decode(msg, data):
  n = msg.impl.decode(array(data, 'b'), 0, len(data))
  msg.post_decode()
  return n

from java.nio import BufferOverflowException

def pn_message_encode(msg, size):
  msg.pre_encode()
  ba = zeros(size, 'b')
  # XXX: shouldn't have to use the try/catch
  try:
    n = msg.impl.encode(ba, 0, size)
    if n >= 0:
      return n, ba[:n].tostring()
    else:
      return n
  except BufferOverflowException, e:
    return PN_OVERFLOW, None

def pn_message_clear(msg):
  msg.impl.clear()
