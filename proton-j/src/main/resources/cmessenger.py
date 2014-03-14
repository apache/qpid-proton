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
from org.apache.qpid.proton.messenger import Messenger, Status
from org.apache.qpid.proton import InterruptException, TimeoutException

from cerror import *

# from proton/messenger.h
PN_STATUS_UNKNOWN = 0
PN_STATUS_PENDING = 1
PN_STATUS_ACCEPTED = 2
PN_STATUS_REJECTED = 3
PN_STATUS_RELEASED = 4
PN_STATUS_MODIFIED = 5
PN_STATUS_ABORTED = 6
PN_STATUS_SETTLED = 7

PN_CUMULATIVE = 1

class pn_messenger_wrapper:

  def __init__(self, impl):
    self.impl = impl
    self.error = pn_error(0, None)

def pn_messenger(name):
  if name is None:
    return pn_messenger_wrapper(Proton.messenger())
  else:
    return pn_messenger_wrapper(Proton.messenger(name))

def pn_messenger_error(m):
  return m.error

def pn_messenger_set_timeout(m, t):
  m.impl.setTimeout(t)
  return 0

def pn_messenger_set_blocking(m, b):
  m.impl.setBlocking(b)
  return 0

def pn_messenger_set_certificate(m, c):
  m.impl.setCertificate(c)
  return 0

def pn_messenger_set_private_key(m, p):
  m.impl.setPrivateKey(p)
  return 0

def pn_messenger_set_password(m, p):
  m.impl.setPassword(p)
  return 0

def pn_messenger_set_trusted_certificates(m, t):
  m.impl.setTrustedCertificates(t)
  return 0

def pn_messenger_set_incoming_window(m, w):
  m.impl.setIncomingWindow(w)
  return 0

def pn_messenger_set_outgoing_window(m, w):
  m.impl.setOutgoingWindow(w)
  return 0

def pn_messenger_start(m):
  m.impl.start()
  return 0

# XXX: ???
def pn_messenger_work(m, t):
  try:
    if m.impl.work(t):
      return 1
    else:
      return PN_TIMEOUT
  except InterruptException, e:
    return PN_INTR

class pn_subscription:

  def __init__(self):
    pass

def pn_messenger_subscribe(m, source):
  m.impl.subscribe(source)
  return pn_subscription()

def pn_messenger_route(m, pattern, address):
  m.impl.route(pattern, address)
  return 0

def pn_messenger_rewrite(m, pattern, address):
  m.impl.rewrite(pattern, address)
  return 0

def pn_messenger_interrupt(m):
  m.impl.interrupt()
  return 0

def pn_messenger_buffered(m, t):
  raise Skipped()

from org.apache.qpid.proton.engine import TransportException

def pn_messenger_stop(m):
  m.impl.stop()
  return 0

def pn_messenger_stopped(m):
  return m.impl.stopped()

def pn_messenger_put(m, msg):
  msg.pre_encode()
  m.impl.put(msg.impl)
  return 0

def pn_messenger_outgoing_tracker(m):
  return m.impl.outgoingTracker()

def pn_messenger_send(m, n):
  try:
    m.impl.send(n)
    return 0
  except InterruptException, e:
    return PN_INTR
  except TimeoutException, e:
    return PN_TIMEOUT

def pn_messenger_recv(m, n):
  try:
    m.impl.recv(n)
    return 0
  except InterruptException, e:
    return PN_INTR
  except TimeoutException, e:
    return PN_TIMEOUT

def pn_messenger_receiving(m):
  return m.impl.receiving()

def pn_messenger_incoming(m):
  return m.impl.incoming()

def pn_messenger_outgoing(m):
  return m.impl.outgoing()

def pn_messenger_get(m, msg):
  mimpl = m.impl.get()
  if msg:
    msg.decode(mimpl)
  return 0

def pn_messenger_incoming_tracker(m):
  return m.impl.incomingTracker()

def pn_messenger_accept(m, tracker, flags):
  if flags:
    m.impl.accept(tracker, Messenger.CUMULATIVE)
  else:
    m.impl.accept(tracker, 0)
  return 0

def pn_messenger_reject(m, tracker, flags):
  if flags:
    m.impl.reject(tracker, Messenger.CUMULATIVE)
  else:
    m.impl.reject(tracker, 0)
  return 0

def pn_messenger_settle(m, tracker, flags):
  if flags:
    m.impl.settle(tracker, Messenger.CUMULATIVE)
  else:
    m.impl.settle(tracker, 0)
  return 0

STATUS_P2J = {
  PN_STATUS_UNKNOWN: Status.UNKNOWN,
  PN_STATUS_PENDING: Status.PENDING,
  PN_STATUS_ACCEPTED: Status.ACCEPTED,
  PN_STATUS_REJECTED: Status.REJECTED,
  PN_STATUS_RELEASED: Status.RELEASED,
  PN_STATUS_MODIFIED: Status.MODIFIED,
  PN_STATUS_ABORTED: Status.ABORTED,
  PN_STATUS_SETTLED: Status.SETTLED
}

STATUS_J2P = {
  Status.UNKNOWN: PN_STATUS_UNKNOWN,
  Status.PENDING: PN_STATUS_PENDING,
  Status.ACCEPTED: PN_STATUS_ACCEPTED,
  Status.REJECTED: PN_STATUS_REJECTED,
  Status.RELEASED: PN_STATUS_RELEASED,
  Status.MODIFIED: PN_STATUS_MODIFIED,
  Status.ABORTED: PN_STATUS_ABORTED,
  Status.SETTLED: PN_STATUS_SETTLED
}

def pn_messenger_status(m, tracker):
  return STATUS_J2P[m.impl.getStatus(tracker)]

def pn_messenger_set_passive(m, passive):
  raise Skipped()

def pn_messenger_selectable(m):
  raise Skipped()
