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
from __future__ import absolute_import

import sys, os
from . import common
from . import engine

from proton import *
from .common import pump, Skipped
from proton._compat import str2bin

def _sslCertpath(file):
    """ Return the full path to the certificate,keyfile, etc.
    """
    if os.name=="nt":
        if file.find("private-key")!=-1:
            # The private key is not in a separate store
            return None
        # Substitute pkcs#12 equivalent for the CA/key store
        if file.endswith(".pem"):
            file = file[:-4] + ".p12"
    return os.path.join(os.path.dirname(__file__),
                        "ssl_db/%s" % file)

def _testSaslMech(self, mech, clientUser='user@proton', authUser='user@proton', encrypted=False, authenticated=True):
  self.s1.allowed_mechs(mech)
  self.c1.open()
  self.c2.open()

  pump(self.t1, self.t2, 1024)

  if encrypted is not None:
    assert self.t2.encrypted == encrypted, encrypted
    assert self.t1.encrypted == encrypted, encrypted

  assert self.t2.authenticated == authenticated, authenticated
  assert self.t1.authenticated == authenticated, authenticated
  if authenticated:
    # Server
    assert self.t2.user == authUser
    assert self.s2.user == authUser
    assert self.s2.mech == mech.strip()
    assert self.s2.outcome == SASL.OK, self.s2.outcome
    assert self.c2.state & Endpoint.LOCAL_ACTIVE and self.c2.state & Endpoint.REMOTE_ACTIVE,\
      "local_active=%s, remote_active=%s" % (self.c1.state & Endpoint.LOCAL_ACTIVE, self.c1.state & Endpoint.REMOTE_ACTIVE)
    # Client
    assert self.t1.user == clientUser
    assert self.s1.user == clientUser
    assert self.s1.mech == mech.strip()
    assert self.s1.outcome == SASL.OK, self.s1.outcome
    assert self.c1.state & Endpoint.LOCAL_ACTIVE and self.c1.state & Endpoint.REMOTE_ACTIVE,\
      "local_active=%s, remote_active=%s" % (self.c1.state & Endpoint.LOCAL_ACTIVE, self.c1.state & Endpoint.REMOTE_ACTIVE)
  else:
    # Server
    assert self.t2.user == None
    assert self.s2.user == None
    assert self.s2.outcome != SASL.OK, self.s2.outcome
    # Client
    assert self.t1.user == clientUser
    assert self.s1.user == clientUser
    assert self.s1.outcome != SASL.OK, self.s1.outcome

class Test(common.Test):
  pass

class SaslTest(Test):

  def setUp(self):
    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport(Transport.SERVER)
    self.t2.max_frame_size = 65536
    self.s2 = SASL(self.t2)

  def pump(self):
    pump(self.t1, self.t2, 1024)

  # Note that due to server protocol autodetect, there can be no "pipelining"
  # of protocol frames from the server end only from the client end.
  #
  # This is because the server cannot know which protocol layers are active
  # and therefore which headers need to be sent,
  # until it sees the respective protocol headers from the client.
  def testPipelinedClient(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support client pipelining")

    # Client
    self.s1.allowed_mechs('ANONYMOUS')
    # Server
    self.s2.allowed_mechs('ANONYMOUS')

    assert self.s1.outcome is None
    assert self.s2.outcome is None

    # Push client bytes into server
    out1 = self.t1.peek(1024)
    self.t1.pop(len(out1))
    self.t2.push(out1)

    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))

    assert self.s1.outcome is None

    self.t1.push(out2)

    assert self.s1.outcome == SASL.OK
    assert self.s2.outcome == SASL.OK

  def testPipelinedClientFail(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support client pipelining")

    # Client
    self.s1.allowed_mechs('ANONYMOUS')
    # Server
    self.s2.allowed_mechs('PLAIN DIGEST-MD5 SCRAM-SHA-1')

    assert self.s1.outcome is None
    assert self.s2.outcome is None

    # Push client bytes into server
    out1 = self.t1.peek(1024)
    self.t1.pop(len(out1))
    self.t2.push(out1)

    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))

    assert self.s1.outcome is None

    self.t1.push(out2)

    assert self.s1.outcome == SASL.AUTH
    assert self.s2.outcome == SASL.AUTH

  def testSaslAndAmqpInSingleChunk(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support client pipelining")

    self.s1.allowed_mechs('ANONYMOUS')
    self.s2.allowed_mechs('ANONYMOUS')

    # do some work to generate AMQP data
    c1 = Connection()
    c2 = Connection()
    self.t1.bind(c1)
    c1._transport = self.t1
    self.t2.bind(c2)
    c2._transport = self.t2

    c1.open()

    # get all t1's output in one buffer then pass it all to t2
    out1_sasl_and_amqp = str2bin("")
    t1_still_producing = True
    while t1_still_producing:
      out1 = self.t1.peek(1024)
      self.t1.pop(len(out1))
      out1_sasl_and_amqp += out1
      t1_still_producing = out1

    t2_still_consuming = True
    while t2_still_consuming:
      num = min(self.t2.capacity(), len(out1_sasl_and_amqp))
      self.t2.push(out1_sasl_and_amqp[:num])
      out1_sasl_and_amqp = out1_sasl_and_amqp[num:]
      t2_still_consuming = num > 0 and len(out1_sasl_and_amqp) > 0

    assert len(out1_sasl_and_amqp) == 0, (len(out1_sasl_and_amqp), out1_sasl_and_amqp)

    # check that t2 processed both the SASL data and the AMQP data
    assert self.s2.outcome == SASL.OK
    assert c2.state & Endpoint.REMOTE_ACTIVE

  def testPipelined2(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support client pipelining")

    out1 = self.t1.peek(1024)
    self.t1.pop(len(out1))
    self.t2.push(out1)

    self.s2.allowed_mechs('ANONYMOUS')
    c2 = Connection()
    c2.open()
    self.t2.bind(c2)

    out2 = self.t2.peek(1024)
    self.t2.pop(len(out2))
    self.t1.push(out2)

    out1 = self.t1.peek(1024)
    assert len(out1) > 0

  def testFracturedSASL(self):
    """ PROTON-235
    """
    assert self.s1.outcome is None

    # self.t1.trace(Transport.TRACE_FRM)

    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push(str2bin("AMQP\x03\x01\x00\x00"))
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push(str2bin("\x00\x00\x00"))
    out = self.t1.peek(1024)
    self.t1.pop(len(out))

    self.t1.push(str2bin("6\x02\x01\x00\x00\x00S@\xc04\x01\xe01\x04\xa3\x05PLAIN\x0aDIGEST-MD5\x09ANONYMOUS\x08CRAM-MD5"))
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    self.t1.push(str2bin("\x00\x00\x00\x10\x02\x01\x00\x00\x00SD\xc0\x03\x01P\x00"))
    out = self.t1.peek(1024)
    self.t1.pop(len(out))
    while out:
      out = self.t1.peek(1024)
      self.t1.pop(len(out))

    assert self.s1.outcome == SASL.OK, self.s1.outcome

  def test_singleton(self):
      """Verify that only a single instance of SASL can exist per Transport"""
      transport = Transport()
      attr = object()
      sasl1 = SASL(transport)
      sasl1.my_attribute = attr
      sasl2 = transport.sasl()
      sasl3 = SASL(transport)
      assert sasl1 == sasl2
      assert sasl1 == sasl3
      assert sasl1.my_attribute == attr
      assert sasl2.my_attribute == attr
      assert sasl3.my_attribute == attr
      transport = Transport()
      sasl1 = transport.sasl()
      sasl1.my_attribute = attr
      sasl2 = SASL(transport)
      assert sasl1 == sasl2
      assert sasl1.my_attribute == attr
      assert sasl2.my_attribute == attr

  def testSaslSkipped(self):
    """Verify that the server (with SASL) correctly handles a client without SASL"""
    self.t1 = Transport()
    self.t2.require_auth(False)
    self.pump()
    assert self.s2.outcome == None
    assert self.t2.condition == None
    assert self.t2.authenticated == False
    assert self.s1.outcome == None
    assert self.t1.condition == None
    assert self.t1.authenticated == False

  def testSaslSkippedFail(self):
    """Verify that the server (with SASL) correctly handles a client without SASL"""
    self.t1 = Transport()
    self.t2.require_auth(True)
    self.pump()
    assert self.s2.outcome == None
    assert self.t2.condition != None
    assert self.s1.outcome == None
    assert self.t1.condition != None

  def testMechNotFound(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support checking authentication state")
    self.c1 = Connection()
    self.c1.open()
    self.t1.bind(self.c1)
    self.s1.allowed_mechs('IMPOSSIBLE')

    self.pump()

    assert self.t2.authenticated == False
    assert self.t1.authenticated == False
    assert self.s1.outcome != SASL.OK
    assert self.s2.outcome != SASL.OK

class SASLMechTest(Test):
  def setUp(self):
    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport(Transport.SERVER)
    self.s2 = SASL(self.t2)

    self.c1 = Connection()
    self.c1.user = 'user@proton'
    self.c1.password = 'password'
    self.c1.hostname = 'localhost'

    self.c2 = Connection()

  def testANON(self):
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'ANONYMOUS', authUser='anonymous')

  def testCRAMMD5(self):
    common.ensureCanTestExtendedSASL()

    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'CRAM-MD5')

  def testDIGESTMD5(self):
    common.ensureCanTestExtendedSASL()

    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5')

  # PLAIN shouldn't work without encryption without special setting
  def testPLAINfail(self):
    common.ensureCanTestExtendedSASL()

    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'PLAIN', authenticated=False)

  # Client won't accept PLAIN even if offered by server without special setting
  def testPLAINClientFail(self):
    common.ensureCanTestExtendedSASL()

    self.s2.allow_insecure_mechs = True
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'PLAIN', authenticated=False)

  # PLAIN will only work if both ends are specially set up
  def testPLAIN(self):
    common.ensureCanTestExtendedSASL()

    self.s1.allow_insecure_mechs = True
    self.s2.allow_insecure_mechs = True
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'PLAIN')

# SCRAM not supported before Cyrus SASL 2.1.26
# so not universal and hance need a test for support
# to keep it in tests.
#  def testSCRAMSHA1(self):
#    common.ensureCanTestExtendedSASL()
#
#    self.t1.bind(self.c1)
#    self.t2.bind(self.c2)
#    _testSaslMech(self, 'SCRAM-SHA-1')

def _sslConnection(domain, transport, connection):
  transport.bind(connection)
  ssl = SSL(transport, domain, None )
  return connection

class SSLSASLTest(Test):
  def setUp(self):
    if not common.isSSLPresent():
      raise Skipped("No SSL libraries found.")

    self.server_domain = SSLDomain(SSLDomain.MODE_SERVER)
    self.client_domain = SSLDomain(SSLDomain.MODE_CLIENT)

    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport(Transport.SERVER)
    self.s2 = SASL(self.t2)

    self.c1 = Connection()
    self.c2 = Connection()

  def testSSLPlainSimple(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support SSL with SASL")
    if not SASL.extended():
      raise Skipped("Simple SASL server does not support PLAIN")
    common.ensureCanTestExtendedSASL()

    clientUser = 'user@proton'
    mech = 'PLAIN'

    self.c1.user = clientUser
    self.c1.password = 'password'
    self.c1.hostname = 'localhost'

    ssl1 = _sslConnection(self.client_domain, self.t1, self.c1)
    ssl2 = _sslConnection(self.server_domain, self.t2, self.c2)

    _testSaslMech(self, mech, encrypted=True)

  def testSSLPlainSimpleFail(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support SSL with SASL")
    if not SASL.extended():
      raise Skipped("Simple SASL server does not support PLAIN")
    common.ensureCanTestExtendedSASL()

    clientUser = 'usr@proton'
    mech = 'PLAIN'

    self.c1.user = clientUser
    self.c1.password = 'password'
    self.c1.hostname = 'localhost'

    ssl1 = _sslConnection(self.client_domain, self.t1, self.c1)
    ssl2 = _sslConnection(self.server_domain, self.t2, self.c2)

    _testSaslMech(self, mech, clientUser='usr@proton', encrypted=True, authenticated=False)

  def testSSLExternalSimple(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support SSL with SASL")

    if os.name=="nt":
      extUser = 'O=Client, CN=127.0.0.1'
    else:
      extUser = 'O=Client,CN=127.0.0.1'
    mech = 'EXTERNAL'

    self.server_domain.set_credentials(_sslCertpath("server-certificate.pem"),
                                       _sslCertpath("server-private-key.pem"),
                                       "server-password")
    self.server_domain.set_trusted_ca_db(_sslCertpath("ca-certificate.pem"))
    self.server_domain.set_peer_authentication(SSLDomain.VERIFY_PEER,
                                               _sslCertpath("ca-certificate.pem") )
    self.client_domain.set_credentials(_sslCertpath("client-certificate.pem"),
                                       _sslCertpath("client-private-key.pem"),
                                       "client-password")
    self.client_domain.set_trusted_ca_db(_sslCertpath("ca-certificate.pem"))
    self.client_domain.set_peer_authentication(SSLDomain.VERIFY_PEER)

    ssl1 = _sslConnection(self.client_domain, self.t1, self.c1)
    ssl2 = _sslConnection(self.server_domain, self.t2, self.c2)

    _testSaslMech(self, mech, clientUser=None, authUser=extUser, encrypted=True)

  def testSSLExternalSimpleFail(self):
    if "java" in sys.platform:
      raise Skipped("Proton-J does not support SSL with SASL")

    mech = 'EXTERNAL'

    self.server_domain.set_credentials(_sslCertpath("server-certificate.pem"),
                                       _sslCertpath("server-private-key.pem"),
                                       "server-password")
    self.server_domain.set_trusted_ca_db(_sslCertpath("ca-certificate.pem"))
    self.server_domain.set_peer_authentication(SSLDomain.VERIFY_PEER,
                                               _sslCertpath("ca-certificate.pem") )
    self.client_domain.set_trusted_ca_db(_sslCertpath("ca-certificate.pem"))
    self.client_domain.set_peer_authentication(SSLDomain.VERIFY_PEER)

    ssl1 = _sslConnection(self.client_domain, self.t1, self.c1)
    ssl2 = _sslConnection(self.server_domain, self.t2, self.c2)

    _testSaslMech(self, mech, clientUser=None, authUser=None, encrypted=None, authenticated=False)

class SASLEventTest(engine.CollectorTest):
  def setUp(self):
    engine.CollectorTest.setUp(self)
    self.t1 = Transport()
    self.s1 = SASL(self.t1)
    self.t2 = Transport(Transport.SERVER)
    self.s2 = SASL(self.t2)

    self.c1 = Connection()
    self.c1.user = 'user@proton'
    self.c1.password = 'password'
    self.c1.hostname = 'localhost'

    self.c2 = Connection()

    self.collector = Collector()

  def testNormalAuthenticationClient(self):
    common.ensureCanTestExtendedSASL()
    self.c1.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5')
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.CONNECTION_REMOTE_OPEN)

  def testNormalAuthenticationServer(self):
    common.ensureCanTestExtendedSASL()
    self.c2.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5')
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.CONNECTION_REMOTE_OPEN)

  def testFailedAuthenticationClient(self):
    common.ensureCanTestExtendedSASL()
    clientUser = "usr@proton"
    self.c1.user = clientUser
    self.c1.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5', clientUser=clientUser, authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_ERROR,
                Event.TRANSPORT_TAIL_CLOSED,
                Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testFailedAuthenticationServer(self):
    common.ensureCanTestExtendedSASL()
    clientUser = "usr@proton"
    self.c1.user = clientUser
    self.c2.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5', clientUser=clientUser, authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_ERROR,
                Event.TRANSPORT_TAIL_CLOSED,
                Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testNoMechClient(self):
    common.ensureCanTestExtendedSASL()
    self.c1.collect(self.collector)
    self.s2.allowed_mechs('IMPOSSIBLE')
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_ERROR,
                Event.TRANSPORT_TAIL_CLOSED, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testNoMechServer(self):
    common.ensureCanTestExtendedSASL()
    self.c2.collect(self.collector)
    self.s2.allowed_mechs('IMPOSSIBLE')
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'DIGEST-MD5', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_TAIL_CLOSED,
                Event.TRANSPORT_ERROR, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testDisallowedMechClient(self):
    self.c1.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'IMPOSSIBLE', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_ERROR,
                Event.TRANSPORT_TAIL_CLOSED, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testDisallowedMechServer(self):
    self.c2.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'IMPOSSIBLE', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_TAIL_CLOSED,
                Event.TRANSPORT_ERROR, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testDisallowedPlainClient(self):
    self.c1.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'PLAIN', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_ERROR,
                Event.TRANSPORT_TAIL_CLOSED, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)

  def testDisallowedPlainServer(self):
    self.c2.collect(self.collector)
    self.t1.bind(self.c1)
    self.t2.bind(self.c2)
    _testSaslMech(self, 'PLAIN', authenticated=False)
    self.expect(Event.CONNECTION_INIT, Event.CONNECTION_BOUND,
                Event.CONNECTION_LOCAL_OPEN, Event.TRANSPORT,
                Event.TRANSPORT_TAIL_CLOSED,
                Event.TRANSPORT_ERROR, Event.TRANSPORT_HEAD_CLOSED, Event.TRANSPORT_CLOSED)
