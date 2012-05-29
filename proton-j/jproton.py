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
from jarray import zeros
from java.util import EnumSet

PN_EOS = Transport.END_OF_STREAM

PN_LOCAL_UNINIT = 1
PN_LOCAL_ACTIVE = 2
PN_LOCAL_CLOSED = 4
PN_REMOTE_UNINIT = 8
PN_REMOTE_ACTIVE = 16
PN_REMOTE_CLOSED = 32

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

def pn_connection_state(c):
  return state(c)

def pn_connection_open(c):
  return c.open()

def pn_connection_close(c):
  return c.close()

def pn_connection_destroy(c):
  pass

def pn_session(c):
  return c.session()

def pn_session_state(s):
  return state(s)

def pn_session_open(s):
  return s.open()

def pn_transport(c):
  return c.transport()

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
