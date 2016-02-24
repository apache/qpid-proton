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

from compat import array, zeros

from cerror import *

# from proton/sasl.h
PN_SASL_NONE=-1
PN_SASL_OK=0
PN_SASL_AUTH=1
PN_SASL_SYS=2
PN_SASL_PERM=3
PN_SASL_TEMP=4

def pn_sasl_extended():
  return False

def pn_sasl(tp):
  sasl = tp.impl.sasl()
  if tp.server:
    sasl.server()
  else:
    sasl.client()
  return sasl

SASL_OUTCOMES_P2J = {
  PN_SASL_NONE: Sasl.PN_SASL_NONE,
  PN_SASL_OK: Sasl.PN_SASL_OK,
  PN_SASL_AUTH: Sasl.PN_SASL_AUTH,
  PN_SASL_SYS: Sasl.PN_SASL_SYS,
  PN_SASL_PERM: Sasl.PN_SASL_PERM,
  PN_SASL_TEMP: Sasl.PN_SASL_TEMP,
}

SASL_OUTCOMES_J2P = {
  Sasl.PN_SASL_NONE: PN_SASL_NONE,
  Sasl.PN_SASL_OK: PN_SASL_OK,
  Sasl.PN_SASL_AUTH: PN_SASL_AUTH,
  Sasl.PN_SASL_SYS: PN_SASL_SYS,
  Sasl.PN_SASL_PERM: PN_SASL_PERM,
  Sasl.PN_SASL_TEMP: PN_SASL_TEMP,
}

def pn_transport_require_auth(transport, require):
  raise Skipped('Not supported in Proton-J')

# TODO: Placeholders
def pn_transport_is_authenticated(transport):
  raise Skipped('Not supported in Proton-J')

def pn_transport_is_encrypted(transport):
  raise Skipped('Not supported in Proton-J')

def pn_transport_get_user(transport):
  raise Skipped('Not supported in Proton-J')

def pn_connection_set_user(connection, user):
  pass

def pn_connection_set_password(connection, password):
  pass

def pn_sasl_allowed_mechs(sasl, mechs):
  sasl.setMechanisms(*mechs.split())

def pn_sasl_set_allow_insecure_mechs(sasl, insecure):
  pass

def pn_sasl_done(sasl, outcome):
  sasl.done(SASL_OUTCOMES_P2J[outcome])

def pn_sasl_outcome(sasl):
  return SASL_OUTCOMES_J2P[sasl.getOutcome()]
