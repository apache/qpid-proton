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

import os, common
import subprocess
from proton import *


class SslTest(common.Test):

    def __init__(self, *args):
        common.Test.__init__(self, *args)

    def setup(self):
        self.t_server = Transport()
        self.server = SSL(self.t_server)
        self.server.init(SSL.MODE_SERVER)
        self.t_client = Transport()
        self.client = SSL(self.t_client)
        self.client.init(SSL.MODE_CLIENT)

    def teardown(self):
        self.t_client = None
        self.t_server = None

    def _pump(self):
        while True:
            out_client = self.t_client.output(1024)
            out_server = self.t_server.output(1024)
            if out_client: self.t_server.input(out_client)
            if out_server: self.t_client.input(out_server)
            if not out_client and not out_server: break

    def _testpath(self, file):
        """ Set the full path to the certificate,keyfile, etc. for the test.
        """
        return os.path.join(os.path.dirname(__file__),
                            "ssl_db/%s" % file)

    def test_defaults(self):
        """ By default, both the server and the client support anonymous
        ciphers - they should connect without need for a certificate.
        """
        client_conn = Connection()
        self.t_client.bind(client_conn)
        server_conn = Connection()
        self.t_server.bind(server_conn)

        # check that no SSL connection exists
        assert not self.server.cipher_name()
        assert not self.client.protocol_name()

        client_conn.open()
        server_conn.open()
        self._pump()

        # now SSL should be active
        assert self.server.cipher_name() is not None
        assert self.client.protocol_name() is not None

        client_conn.close()
        server_conn.close()
        self._pump()


    def test_server_certificate(self):
        """ Test that anonymous clients can still connect to a server that has
        a certificate configured.
        """
        self.server.set_credentials(self._testpath("server-certificate.pem"),
                                    self._testpath("server-private-key.pem"),
                                    "server-password")
        client_conn = Connection()
        self.t_client.bind(client_conn)
        server_conn = Connection()
        self.t_server.bind(server_conn)
        client_conn.open()
        server_conn.open()
        self._pump()
        assert self.client.protocol_name() is not None
        client_conn.close()
        server_conn.close()
        self._pump()

    def test_server_authentication(self):
        """ Simple SSL connection with authentication of the server
        """
        self.server.set_credentials(self._testpath("server-certificate.pem"),
                                    self._testpath("server-private-key.pem"),
                                    "server-password")

        self.client.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client.set_peer_authentication( SSL.VERIFY_PEER )

        client_conn = Connection()
        self.t_client.bind(client_conn)
        server_conn = Connection()
        self.t_server.bind(server_conn)
        client_conn.open()
        server_conn.open()
        self._pump()
        assert self.client.protocol_name() is not None
        client_conn.close()
        server_conn.close()
        self._pump()

    def test_client_authentication(self):
        """ Force the client to authenticate.
        """
        # note: when requesting client auth, the server _must_ send its
        # certificate, so make sure we configure one!
        self.server.set_credentials(self._testpath("server-certificate.pem"),
                                    self._testpath("server-private-key.pem"),
                                    "server-password")
        self.server.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server.set_peer_authentication( SSL.VERIFY_PEER,
                                             self._testpath("ca-certificate.pem") )

        # give the client a certificate, but let's not require server authentication
        self.client.set_credentials(self._testpath("client-certificate.pem"),
                                    self._testpath("client-private-key.pem"),
                                    "client-password")
        self.client.set_peer_authentication( SSL.ANONYMOUS_PEER )

        client_conn = Connection()
        self.t_client.bind(client_conn)
        server_conn = Connection()
        self.t_server.bind(server_conn)
        client_conn.open()
        server_conn.open()
        self._pump()
        assert self.client.protocol_name() is not None
        client_conn.close()
        server_conn.close()
        self._pump()


    def test_client_server_authentication(self):
        """ Require both client and server to mutually identify themselves.
        """
        self.server.set_credentials(self._testpath("server-certificate.pem"),
                                    self._testpath("server-private-key.pem"),
                                    "server-password")
        self.server.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.server.set_peer_authentication( SSL.VERIFY_PEER,
                                             self._testpath("ca-certificate.pem") )

        self.client.set_credentials(self._testpath("client-certificate.pem"),
                                    self._testpath("client-private-key.pem"),
                                    "client-password")
        self.client.set_trusted_ca_db(self._testpath("ca-certificate.pem"))
        self.client.set_peer_authentication( SSL.VERIFY_PEER )

        client_conn = Connection()
        self.t_client.bind(client_conn)
        server_conn = Connection()
        self.t_server.bind(server_conn)
        client_conn.open()
        server_conn.open()
        self._pump()
        assert self.client.protocol_name() is not None
        client_conn.close()
        server_conn.close()
        self._pump()


