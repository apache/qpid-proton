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
from org.apache.qpid.proton.engine import SslDomain

from cerror import *

# from proton/ssl.h
PN_SSL_MODE_CLIENT = 1
PN_SSL_MODE_SERVER = 2

PN_SSL_RESUME_UNKNOWN = 0
PN_SSL_RESUME_NEW = 1
PN_SSL_RESUME_REUSED = 2

PN_SSL_VERIFY_NULL=0
PN_SSL_VERIFY_PEER=1
PN_SSL_ANONYMOUS_PEER=2
PN_SSL_VERIFY_PEER_NAME=3

PN_SSL_MODE_J2P = {
  SslDomain.Mode.CLIENT: PN_SSL_MODE_CLIENT,
  SslDomain.Mode.SERVER: PN_SSL_MODE_SERVER
}

PN_SSL_MODE_P2J = {
  PN_SSL_MODE_CLIENT: SslDomain.Mode.CLIENT,
  PN_SSL_MODE_SERVER: SslDomain.Mode.SERVER
}

def pn_ssl_domain(mode):
  domain = Proton.sslDomain()
  domain.init(PN_SSL_MODE_P2J[mode])
  return domain

def pn_ssl_domain_set_credentials(domain, certificate_file, private_key_file, password):
  domain.setCredentials(certificate_file, private_key_file, password)
  return 0

def pn_ssl_domain_set_trusted_ca_db(domain, trusted_db):
  domain.setTrustedCaDb(trusted_db)
  return 0

PN_VERIFY_MODE_J2P = {
  None: PN_SSL_VERIFY_NULL,
  SslDomain.VerifyMode.VERIFY_PEER: PN_SSL_VERIFY_PEER,
  SslDomain.VerifyMode.VERIFY_PEER_NAME: PN_SSL_VERIFY_PEER_NAME,
  SslDomain.VerifyMode.ANONYMOUS_PEER: PN_SSL_ANONYMOUS_PEER
}

PN_VERIFY_MODE_P2J = {
  PN_SSL_VERIFY_NULL: None,
  PN_SSL_VERIFY_PEER: SslDomain.VerifyMode.VERIFY_PEER,
  PN_SSL_VERIFY_PEER_NAME: SslDomain.VerifyMode.VERIFY_PEER_NAME,
  PN_SSL_ANONYMOUS_PEER: SslDomain.VerifyMode.ANONYMOUS_PEER
}

def pn_ssl_domain_set_peer_authentication(domain, mode, trusted=None):
  domain.setPeerAuthentication(PN_VERIFY_MODE_P2J[mode])
  if trusted:
    domain.setTrustedCaDb(trusted)
  return 0

def pn_ssl_domain_allow_unsecured_client(domain):
  domain.allowUnsecuredClient(True)
  return 0

class pn_ssl_wrapper:

  def __init__(self, transport):
    self.impl = None
    self.transport = transport

def pn_ssl(transport):
  if getattr(transport, "ssl", None) is not None:
    return transport.ssl
  else:
    transport.ssl = pn_ssl_wrapper(transport)
    return transport.ssl

def pn_ssl_init(ssl, domain, session_id):
  # XXX: session_id
  ssl.impl = ssl.transport.impl.ssl(domain, None)

def pn_ssl_resume_status(ssl):
  raise Skipped()

def pn_ssl_get_cipher_name(ssl, size):
  name = ssl.impl.getCipherName()
  return (bool(name), name)

def pn_ssl_get_protocol_name(ssl, size):
  name = ssl.impl.getProtocolName()
  return (bool(name), name)

