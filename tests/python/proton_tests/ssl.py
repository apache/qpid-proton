from __future__ import absolute_import
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

import os
from . import common
import random
import string
import subprocess
import sys
from proton import *
from .common import Skipped, pump


def _testpath(file):
    """ Set the full path to the certificate,keyfile, etc. for the test.
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

class SslTest(common.Test):

    def __init__(self, *args):
        common.Test.__init__(self, *args)
        self._testpath = _testpath

    def setUp(self):
        if not common.isSSLPresent():
            raise Skipped("No SSL libraries found.")
        self.server_domain = SSLDomain(SSLDomain.MODE_SERVER)
        self.client_domain = SSLDomain(SSLDomain.MODE_CLIENT)

    def tearDown(self):
        self.server_domain = None
        self.client_domain = None

    class SslTestConnection(object):
        """ Represents a single SSL connection.
        """
        def __init__(self, domain=None, mode=Transport.CLIENT,
                     session_details=None, conn_hostname=None,
                     ssl_peername=None):
            if not common.isSSLPresent():
                raise Skipped("No SSL libraries found.")

            self.ssl = None
            self.domain = domain
            self.transport = Transport(mode)
            self.connection = Connection()
            if conn_hostname:
                self.connection.hostname = conn_hostname
            if domain:
                self.ssl = SSL( self.transport, self.domain, session_details )
                if ssl_peername:
                    self.ssl.peer_hostname = ssl_peername
            # bind last, after all configuration complete:
            self.transport.bind(self.connection)

    def _pump(self, ssl_client, ssl_server, buffer_size=1024):
        pump(ssl_client.transport, ssl_server.transport, buffer_size)

    def _do_handshake(self, client, server):
        """ Attempt to connect client to server. Will throw a TransportException if the SSL
        handshake fails.
        """
        client.connection.open()
        server.connection.open()
        self._pump(client, server)
        if client.transport.closed:
            return
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump(client, server)

    def test_defaults(self):
        if os.name=="nt":
            raise Skipped("Windows SChannel lacks anonymous cipher support.")
        """ By default, both the server and the client support anonymous
        ciphers - they should connect without need for a certificate.
        """
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        # check that no SSL connection exists
        assert not server.ssl.cipher_name()
        assert not client.ssl.protocol_name()

        #client.transport.trace(Transport.TRACE_DRV)
        #server.transport.trace(Transport.TRACE_DRV)

        client.connection.open()
        server.connection.open()
        self._pump( client, server )

        # now SSL should be active
        assert server.ssl.cipher_name() is not None
        assert client.ssl.protocol_name() is not None

        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_ssl_with_small_buffer(self):
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()

        small_buffer_size = 1
        self._pump( client, server, small_buffer_size )

        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )


    def test_server_certificate(self):
        """ Test that anonymous clients can still connect to a server that has
        a certificate configured.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_server_authentication(self):
        """ Simple SSL connection with authentication of the server
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")

        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_certificate_fingerprint_and_subfields(self):
        if os.name=="nt":
            raise Skipped("Windows support for certificate fingerprint and subfield not implemented yet")

        if "java" in sys.platform:
            raise Skipped("Not yet implemented in Proton-J")

        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        # give the client a certificate, but let's not require server authentication
        self.client_domain.set_credentials(self._testpath("client-certificate1.pem"),
                                           self._testpath("client-private-key1.pem"),
                                           "client-password")
        self.client_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )

        # Test the subject subfields
        self.assertEqual("Client", server.ssl.get_cert_organization())
        self.assertEqual("Dev", server.ssl.get_cert_organization_unit())
        self.assertEqual("ST", server.ssl.get_cert_state_or_province())
        self.assertEqual("US", server.ssl.get_cert_country())
        self.assertEqual("City", server.ssl.get_cert_locality_or_city())
        self.assertEqual("O=Server,CN=A1.Good.Server.domain.com", client.ssl.get_cert_subject())
        self.assertEqual("O=Client,CN=127.0.0.1,C=US,ST=ST,L=City,OU=Dev", server.ssl.get_cert_subject())

        self.assertEqual("03c97341abafe5861d6969c66a190d2846d905d6", server.ssl.get_cert_fingerprint_sha1())
        self.assertEqual("ad86cc76278e69aef3a5c0dda13fc49831622f92d1364ce1ed361ff842b0143a", server.ssl.get_cert_fingerprint_sha256())
        self.assertEqual("9fb3340ecee4471534d60be025358cae33ef2cc9442ca8bb7703a68db68d9ffb7963678292996011fa55a9a2524b84a40a11a2778f25797e78e23cf05623218d",
                         server.ssl.get_cert_fingerprint_sha512())
        self.assertEqual("0d4faa84a0bb6846eaec6b7493916b30", server.ssl.get_cert_fingerprint_md5())

        # Test the various fingerprint algorithms
        self.assertEqual("f390ddb4dc8a90bcf3528774b622a7f87f7322b5", client.ssl.get_cert_fingerprint_sha1())
        self.assertEqual("c116e902345c99bc01dda14b7a5f1b8ae6a451eddb23e5336c996ba4d12bc122", client.ssl.get_cert_fingerprint_sha256())
        self.assertEqual("8941c8ce00824ab7196bb1952787c90ef7f9bd837cbb0bb4823f57fc89e80033c75adc98b78801928d0035bcd6db6ddc9ab6da026c6548a66ede5c4f43f7e166",
                         client.ssl.get_cert_fingerprint_sha512())
        self.assertEqual("d008bf05afbc983a3f98ae56e3eba643", client.ssl.get_cert_fingerprint_md5())

        self.assertEqual(None, client.ssl.get_cert_fingerprint(21, SSL.SHA1)) # Should be at least 41
        self.assertEqual(None, client.ssl.get_cert_fingerprint(50, SSL.SHA256)) # Should be at least 65
        self.assertEqual(None, client.ssl.get_cert_fingerprint(128, SSL.SHA512)) # Should be at least 129
        self.assertEqual(None, client.ssl.get_cert_fingerprint(10, SSL.MD5)) # Should be at least 33
        self.assertEqual(None, client.ssl._get_cert_subject_unknown_subfield())

        self.assertNotEqual(None, client.ssl.get_cert_fingerprint(50, SSL.SHA1)) # Should be at least 41
        self.assertNotEqual(None, client.ssl.get_cert_fingerprint(70, SSL.SHA256)) # Should be at least 65
        self.assertNotEqual(None, client.ssl.get_cert_fingerprint(130, SSL.SHA512)) # Should be at least 129
        self.assertNotEqual(None, client.ssl.get_cert_fingerprint(35, SSL.MD5)) # Should be at least 33
        self.assertEqual(None, client.ssl._get_cert_fingerprint_unknown_hash_alg())

    def test_client_authentication(self):
        """ Force the client to authenticate.
        """
        # note: when requesting client auth, the server _must_ send its
        # certificate, so make sure we configure one!
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        # give the client a certificate, but let's not require server authentication
        self.client_domain.set_credentials(self._testpath("client-certificate.pem"),
                                           self._testpath("client-private-key.pem"),
                                           "client-password")
        self.client_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )

        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_client_authentication_fail_bad_cert(self):
        """ Ensure that the server can detect a bad client certificate.
        """
        # note: when requesting client auth, the server _must_ send its
        # certificate, so make sure we configure one!
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        self.client_domain.set_credentials(self._testpath("bad-server-certificate.pem"),
                                           self._testpath("bad-server-private-key.pem"),
                                           "server-password")
        self.client_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT

    def test_client_authentication_fail_no_cert(self):
        """ Ensure that the server will fail a client that does not provide a
        certificate.
        """
        # note: when requesting client auth, the server _must_ send its
        # certificate, so make sure we configure one!
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        self.client_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT

    def test_client_server_authentication(self):
        """ Require both client and server to mutually identify themselves.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )

        self.client_domain.set_credentials(self._testpath("client-certificate.pem"),
                                           self._testpath("client-private-key.pem"),
                                           "client-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_server_only_authentication(self):
        """ Client verifies server, but server does not verify client.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )

        self.client_domain.set_credentials(self._testpath("client-certificate.pem"),
                                           self._testpath("client-private-key.pem"),
                                           "client-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_bad_server_certificate(self):
        """ A server with a self-signed certificate that is not trusted by the
        client.  The client should reject the server.
        """
        self.server_domain.set_credentials(self._testpath("bad-server-certificate.pem"),
                                           self._testpath("bad-server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )

        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT

        del server
        del client

        # now re-try with a client that does not require peer verification
        self.client_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )

        client = SslTest.SslTestConnection( self.client_domain )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_allow_unsecured_client_which_connects_unsecured(self):
        """ Server allows an unsecured client to connect if configured.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )
        # allow unsecured clients on this connection
        self.server_domain.allow_unsecured_client()
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        # non-ssl connection
        client = SslTest.SslTestConnection()

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert server.ssl.protocol_name() is None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_allow_unsecured_client_which_connects_secured(self):
        """ As per test_allow_unsecured_client_which_connects_unsecured
            but client actually uses SSL
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )

        self.client_domain.set_credentials(self._testpath("client-certificate.pem"),
                                           self._testpath("client-private-key.pem"),
                                           "client-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        # allow unsecured clients on this connection
        #self.server_domain.allow_unsecured_client()

        # client uses ssl. Server should detect this.
        client = SslTest.SslTestConnection( self.client_domain )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert server.ssl.protocol_name() is not None
        client.connection.close()
        server.connection.close()
        self._pump( client, server )


    def test_disallow_unsecured_client(self):
        """ Non-SSL Client is disallowed from connecting to server.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )

        # non-ssl connection
        client = SslTest.SslTestConnection()

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT

    def test_session_resume(self):
        """ Test resume of client session.
        """
        if os.name=="nt":
            raise Skipped("Windows SChannel session resume not yet implemented.")

        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_peer_authentication( SSLDomain.ANONYMOUS_PEER )

        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        # details will be used in initial and subsequent connections to allow session to be resumed
        initial_session_details = SSLSessionDetails("my-session-id")

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain, session_details=initial_session_details )

        # bring up the connection and store its state
        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert client.ssl.protocol_name() is not None

        # cleanly shutdown the connection
        client.connection.close()
        server.connection.close()
        self._pump( client, server )

        # destroy the existing clients
        del client
        del server

        # now create a new set of connections, use last session id
        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        # provide the details of the last session, allowing it to be resumed
        client = SslTest.SslTestConnection( self.client_domain, session_details=initial_session_details )

        #client.transport.trace(Transport.TRACE_DRV)
        #server.transport.trace(Transport.TRACE_DRV)

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert server.ssl.protocol_name() is not None
        if(API_LANGUAGE=="C"):
            assert client.ssl.resume_status() == SSL.RESUME_REUSED
        else:
            # Java gives no way to check whether a previous session has been resumed
            pass

        client.connection.close()
        server.connection.close()
        self._pump( client, server )

        # now try to resume using an unknown session-id, expect resume to fail
        # and a new session is negotiated

        del client
        del server

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain, session_details=SSLSessionDetails("some-other-session-id") )

        client.connection.open()
        server.connection.open()
        self._pump( client, server )
        assert server.ssl.protocol_name() is not None
        if(API_LANGUAGE=="C"):
            assert client.ssl.resume_status() == SSL.RESUME_NEW

        client.connection.close()
        server.connection.close()
        self._pump( client, server )

    def test_multiple_sessions(self):
        """ Test multiple simultaineous active SSL sessions with bi-directional
        certificate verification, shared across two domains.
        """
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.server_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server_domain.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                    self._testpath("ca-certificate.pem") )

        self.client_domain.set_credentials(self._testpath("client-certificate.pem"),
                                           self._testpath("client-private-key.pem"),
                                           "client-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

        max_count = 100
        sessions = [(SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER ),
                     SslTest.SslTestConnection( self.client_domain )) for x in
                    range(max_count)]
        for s in sessions:
            s[0].connection.open()
            self._pump( s[0], s[1] )

        for s in sessions:
            s[1].connection.open()
            self._pump( s[1], s[0] )
            assert s[0].ssl.cipher_name() is not None
            assert s[1].ssl.cipher_name() == s[0].ssl.cipher_name()

        for s in sessions:
            s[1].connection.close()
            self._pump( s[0], s[1] )

        for s in sessions:
            s[0].connection.close()
            self._pump( s[1], s[0] )

    def test_server_hostname_authentication(self):
        """ Test authentication of the names held in the server's certificate
        against various configured hostnames.
        """
        if os.name=="nt":
            raise Skipped("PROTON-1057: disable temporarily on Windows.")

        # Check the CommonName matches (case insensitive).
        # Assumes certificate contains "CN=A1.Good.Server.domain.com"
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "a1.good.server.domain.com"
        assert client.ssl.peer_hostname == "a1.good.server.domain.com"
        self._do_handshake( client, server )
        del server
        del client
        self.tearDown()

        # Should fail on CN name mismatch:
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                           self._testpath("server-private-key.pem"),
                                           "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "A1.Good.Server.domain.comX"
        self._do_handshake( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT
        del server
        del client
        self.tearDown()

        # Wildcarded Certificate
        # Assumes:
        #   1) certificate contains Server Alternate Names:
        #        "alternate.name.one.com" and "another.name.com"
        #   2) certificate has wildcarded CommonName "*.prefix*.domain.com"
        #

        # Pass: match an alternate
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                           self._testpath("server-wc-private-key.pem"),
                                           "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "alternate.Name.one.com"
        self._do_handshake( client, server )
        del client
        del server
        self.tearDown()

        # Pass: match an alternate
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "ANOTHER.NAME.COM"
        self._do_handshake(client, server)
        del client
        del server
        self.tearDown()

        # Pass: match the pattern
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "SOME.PREfix.domain.COM"
        self._do_handshake( client, server )
        del client
        del server
        self.tearDown()

        # Pass: match the pattern
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "FOO.PREfixZZZ.domain.com"
        self._do_handshake( client, server )
        del client
        del server
        self.tearDown()

        # Fail: must match prefix on wildcard
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "FOO.PREfi.domain.com"
        self._do_handshake( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT
        del server
        del client
        self.tearDown()

        # Fail: leading wildcards are not optional
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection( self.server_domain, mode=Transport.SERVER )
        client = SslTest.SslTestConnection( self.client_domain )

        client.ssl.peer_hostname = "PREfix.domain.COM"
        self._do_handshake( client, server )
        assert client.transport.closed
        assert server.transport.closed
        assert client.connection.state & Endpoint.REMOTE_UNINIT
        assert server.connection.state & Endpoint.REMOTE_UNINIT
        self.tearDown()

        # Pass: ensure that the user can give an alternate name that overrides
        # the connection's configured hostname
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-wc-certificate.pem"),
                                    self._testpath("server-wc-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection(self.server_domain, mode=Transport.SERVER)
        client = SslTest.SslTestConnection(self.client_domain,
                                           conn_hostname="This.Name.Does.not.Match",
                                           ssl_peername="alternate.name.one.com")
        self._do_handshake(client, server)
        del client
        del server
        self.tearDown()

        # Pass: ensure that the hostname supplied by the connection is used if
        # none has been specified for the SSL instanace
        self.setUp()
        self.server_domain.set_credentials(self._testpath("server-certificate.pem"),
                                    self._testpath("server-private-key.pem"),
                                    "server-password")
        self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER_NAME )

        server = SslTest.SslTestConnection(self.server_domain, mode=Transport.SERVER)
        client = SslTest.SslTestConnection(self.client_domain,
                                           conn_hostname="a1.good.server.domain.com")
        self._do_handshake(client, server)
        del client
        del server
        self.tearDown()

    def test_defaults_messenger_app(self):
        """ Test an SSL connection using the Messenger apps (no certificates)
        """
        if os.name=="nt":
            raise Skipped("Windows SChannel lacks anonymous cipher support.")
        port = common.free_tcp_ports()[0]

        receiver = common.MessengerReceiverC()
        receiver.subscriptions = ["amqps://~0.0.0.0:%s" % port]
        receiver.receive_count = 1
        receiver.timeout = self.timeout
        receiver.start()

        sender = common.MessengerSenderC()
        sender.targets = ["amqps://0.0.0.0:%s/X" % port]
        sender.send_count = 1
        sender.timeout = self.timeout
        sender.start()
        sender.wait()
        assert sender.status() == 0, "Command '%s' failed" % str(sender.cmdline())

        receiver.wait()
        assert receiver.status() == 0, "Command '%s' failed" % str(receiver.cmdline())

    def test_server_authentication_messenger_app(self):
        """ Test an SSL authentication using the Messenger apps.
        """
        port = common.free_tcp_ports()[0]

        receiver = common.MessengerReceiverC()
        receiver.subscriptions = ["amqps://~0.0.0.0:%s" % port]
        receiver.receive_count = 1
        receiver.timeout = self.timeout
        # Note hack - by default we use the client-certificate for the
        # _server_ because the client-certificate's common name field
        # is "127.0.0.1", which will match the target address used by
        # the sender.
        receiver.certificate = self._testpath("client-certificate.pem")
        receiver.privatekey = self._testpath("client-private-key.pem")
        receiver.password = "client-password"
        receiver.start()

        sender = common.MessengerSenderC()
        sender.targets = ["amqps://127.0.0.1:%s/X" % port]
        sender.send_count = 1
        sender.timeout = self.timeout
        sender.ca_db = self._testpath("ca-certificate.pem")
        sender.start()
        sender.wait()
        assert sender.status() == 0, "Command '%s' failed" % str(sender.cmdline())

        receiver.wait()
        assert receiver.status() == 0, "Command '%s' failed" % str(receiver.cmdline())

    def DISABLED_test_defaults_valgrind(self):
        """ Run valgrind over a simple SSL connection (no certificates)
        """
        # the openssl libraries produce far too many valgrind errors to be
        # useful.  AFAIK, there is no way to wriate a valgrind suppression
        # expression that will ignore all errors from a given library.
        # Until we can, skip this test.
        port = common.free_tcp_ports()[0]

        receiver = common.MessengerReceiverValgrind()
        receiver.subscriptions = ["amqps://~127.0.0.1:%s" % port]
        receiver.receive_count = 1
        receiver.timeout = self.timeout
        receiver.start()

        sender = common.MessengerSenderValgrind()
        sender.targets = ["amqps://127.0.0.1:%s/X" % port]
        sender.send_count = 1
        sender.timeout = self.timeout
        sender.start()
        sender.wait()
        assert sender.status() == 0, "Command '%s' failed" % str(sender.cmdline())

        receiver.wait()
        assert receiver.status() == 0, "Command '%s' failed" % str(receiver.cmdline())

        # self.server_domain.set_credentials(self._testpath("client-certificate.pem"),
        #                                    self._testpath("client-private-key.pem"),
        #                                    "client-password")

        # self.client_domain.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        # self.client_domain.set_peer_authentication( SSLDomain.VERIFY_PEER )

    def test_singleton(self):
        """Verify that only a single instance of SSL can exist per Transport"""
        transport = Transport()
        ssl1 = SSL(transport, self.client_domain)
        ssl2 = transport.ssl(self.client_domain)
        ssl3 = transport.ssl(self.client_domain)
        assert ssl1 is ssl2
        assert ssl1 is ssl3
        transport = Transport()
        ssl1 = transport.ssl(self.client_domain)
        ssl2 = SSL(transport, self.client_domain)
        assert ssl1 is ssl2
        # catch attempt to re-configure existing SSL
        try:
            ssl3 = SSL(transport, self.server_domain)
            assert False, "Expected error did not occur!"
        except SSLException:
            pass

class MessengerSSLTests(common.Test):

    def setUp(self):
        if not common.isSSLPresent():
            raise Skipped("No SSL libraries found.")
        self.server = Messenger()
        self.client = Messenger()
        self.server.blocking = False
        self.client.blocking = False

    def tearDown(self):
        self.server.stop()
        self.client.stop()
        self.pump()
        assert self.server.stopped
        assert self.client.stopped

    def pump(self, timeout=0):
        while self.client.work(0) or self.server.work(0): pass
        self.client.work(timeout)
        self.server.work(timeout)
        while self.client.work(0) or self.server.work(0): pass

    def test_server_credentials(self,
                                cert="server-certificate.pem",
                                key="server-private-key.pem",
                                password="server-password",
                                exception=None):
        import sys
        # java doesn't do validation in the same way (yet)
        if exception and "java" in sys.platform:
            raise Skipped()
        self.server.certificate = _testpath(cert)
        self.server.private_key = _testpath(key)
        self.server.password = password
        port = common.free_tcp_ports()[0]
        try:
            self.server.start()
            self.server.subscribe("amqps://~0.0.0.0:%s" % port)
            if exception is not None:
                assert False, "expected failure did not occur"
        except MessengerException:
            e = sys.exc_info()[1]
            if exception:
                assert exception in str(e), str(e)
            else:
                raise e

    def test_server_credentials_bad_cert(self):
        self.test_server_credentials(cert="bad",
                                     exception="invalid credentials")

    def test_server_credentials_bad_key(self):
        self.test_server_credentials(key="bad",
                                     exception="invalid credentials")

    def test_server_credentials_bad_password(self):
        self.test_server_credentials(password="bad",
                                     exception="invalid credentials")

    def test_client_credentials(self,
                                trusted="ca-certificate.pem",
                                cert="client-certificate.pem",
                                key="client-private-key.pem",
                                password="client-password",
                                altserv=False,
                                fail=False):
        if altserv:
            self.server.certificate = _testpath("bad-server-certificate.pem")
            self.server.private_key = _testpath("bad-server-private-key.pem")
            self.server.password = "server-password"
        else:
            self.server.certificate = _testpath("client-certificate.pem")
            self.server.private_key = _testpath("client-private-key.pem")
            self.server.password = "client-password"
        self.server.start()
        port = common.free_tcp_ports()[0]
        self.server.subscribe("amqps://~0.0.0.0:%s" % port)
        self.server.incoming_window = 10

        self.client.trusted_certificates = _testpath(trusted)
        self.client.certificate = _testpath(cert)
        self.client.private_key = _testpath(key)
        self.client.password = password
        self.client.outgoing_window = 10
        self.client.start()

        self.server.recv()

        msg = Message()
        msg.address = "amqps://127.0.0.1:%s" % port
        # make sure a large, uncompressible message body works!
        msg.body = "".join(random.choice(string.ascii_letters)
                           for x in range(10099))
        trk = self.client.put(msg)
        self.client.send()

        self.pump()

        if fail:
            assert self.server.incoming == 0, self.server.incoming
            assert self.client.status(trk) == ABORTED, self.client.status(trk)
        else:
            assert self.server.incoming == 1, self.server.incoming

            rmsg = Message()
            self.server.get(rmsg)
            assert rmsg.body == msg.body
            self.server.accept()
            self.pump()

            assert self.client.status(trk) == ACCEPTED, self.client.status(trk)

    def test_client_credentials_bad_cert(self):
        self.test_client_credentials(cert="bad", fail=True)

    def test_client_credentials_bad_trusted(self):
        self.test_client_credentials(trusted="bad", fail=True)

    def test_client_credentials_bad_password(self):
        self.test_client_credentials(password="bad", fail=True)

    def test_client_credentials_untrusted(self):
        self.test_client_credentials(altserv=True, fail=True)
