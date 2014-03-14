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
from org.apache.qpid.proton.engine import Sasl

from jarray import array, zeros

from cerror import *

# from proton/sasl.h
PN_SASL_NONE=-1
PN_SASL_OK=0
PN_SASL_AUTH=1
PN_SASL_SYS=2
PN_SASL_PERM=3
PN_SASL_TEMP=4

PN_SASL_CONF = 0
PN_SASL_IDLE = 1
PN_SASL_STEP = 2
PN_SASL_PASS = 3
PN_SASL_FAIL = 4

def pn_sasl(tp):
  return tp.impl.sasl()

SASL_STATES = {
  Sasl.SaslState.PN_SASL_CONF: PN_SASL_CONF,
  Sasl.SaslState.PN_SASL_IDLE: PN_SASL_IDLE,
  Sasl.SaslState.PN_SASL_STEP: PN_SASL_STEP,
  Sasl.SaslState.PN_SASL_PASS: PN_SASL_PASS,
  Sasl.SaslState.PN_SASL_FAIL: PN_SASL_FAIL
  }

SASL_OUTCOMES_P2J = {
  PN_SASL_NONE: Sasl.PN_SASL_NONE,
  PN_SASL_OK: Sasl.PN_SASL_OK,
  PN_SASL_AUTH: Sasl.PN_SASL_AUTH,
  PN_SASL_SYS: Sasl.PN_SASL_SYS,
  PN_SASL_PERM: Sasl.PN_SASL_PERM,
  PN_SASL_TEMP: Sasl.PN_SASL_TEMP
}

SASL_OUTCOMES_J2P = {
  Sasl.PN_SASL_NONE: PN_SASL_NONE,
  Sasl.PN_SASL_OK: PN_SASL_OK,
  Sasl.PN_SASL_AUTH: PN_SASL_AUTH,
  Sasl.PN_SASL_SYS: PN_SASL_SYS,
  Sasl.PN_SASL_PERM: PN_SASL_PERM,
  Sasl.PN_SASL_TEMP: PN_SASL_TEMP
}

def pn_sasl_state(sasl):
  return SASL_STATES[sasl.getState()]

def pn_sasl_mechanisms(sasl, mechs):
  sasl.setMechanisms(*mechs.split())

def pn_sasl_client(sasl):
  sasl.client()

def pn_sasl_server(sasl):
  sasl.server()

def pn_sasl_done(sasl, outcome):
  sasl.done(SASL_OUTCOMES_P2J[outcome])

def pn_sasl_outcome(sasl):
  return SASL_OUTCOMES_J2P[sasl.getOutcome()]

def pn_sasl_plain(sasl, user, password):
  sasl.plain(user, password)

def pn_sasl_recv(sasl, size):
  if size < sasl.pending():
    return PN_OVERFLOW, None
  else:
    ba = zeros(size, 'b')
    n = sasl.recv(ba, 0, size)
    if n >= 0:
      return n, ba[:n].tostring()
    else:
      return n, None

def pn_sasl_send(sasl, data, size):
  return sasl.send(array(data, 'b'), 0, size)
