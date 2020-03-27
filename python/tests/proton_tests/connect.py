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

import time
import sys
import json
from .common import Test, SkipTest, TestServer, free_tcp_port, ensureCanTestExtendedSASL
from proton import SSLDomain
from proton.reactor import Container
from proton.handlers import MessagingHandler
from .ssl import _testpath

def write_connect_conf(obj):
    with open('connect.json', 'w') as outfile:
        json.dump(obj, outfile)

class Server(MessagingHandler):
    def __init__(self, expected_user=None, scheme='amqps'):
        super(Server, self).__init__()
        self.port = free_tcp_port()
        self.scheme = scheme
        self.url = '%s://localhost:%i' % (self.scheme, self.port)
        self.expected_user = expected_user
        self.verified_user = False

    def on_start(self, event):
        self.listener = event.container.listen(self.url)

    def on_connection_opening(self, event):
        if self.expected_user:
            assert event.connection.transport.user == self.expected_user
            self.verified_user = True

    def on_connection_closing(self, event):
        event.connection.close()
        self.listener.close()

class Client(MessagingHandler):
    def __init__(self):
        super(Client, self).__init__()
        self.opened = False

    def on_connection_opened(self, event):
        self.opened = True
        event.connection.close()

class ConnectConfigTest(Test):
    def test_port(self):
        ensureCanTestExtendedSASL()
        server = Server()
        container = Container(server)
        client = Client()
        write_connect_conf({'port':server.port})
        container.connect(handler=client, reconnect=False)
        container.run()
        assert client.opened == True

    def test_user(self):
        ensureCanTestExtendedSASL()
        user = 'user@proton'
        password = 'password'
        server = Server(user)
        container = Container(server)
        client = Client()
        write_connect_conf({'port':server.port, 'user':user, 'password':password})
        container.connect(handler=client, reconnect=False)
        container.run()
        assert client.opened == True
        assert server.verified_user == True

    def test_ssl(self):
        ensureCanTestExtendedSASL()
        server = Server(scheme='amqps')
        container = Container(server)
        container.ssl.server.set_credentials(_testpath('server-certificate.pem'),
                                             _testpath('server-private-key.pem'),
                                             'server-password')
        client = Client()
        config = {
            'scheme':'amqps',
            'port':server.port,
            'tls': {
                'verify': False
             }
        }
        write_connect_conf(config)
        container.connect(handler=client, reconnect=False)
        container.run()
        assert client.opened == True

    def test_ssl_external(self):
        ensureCanTestExtendedSASL()
        server = Server(scheme='amqps')
        container = Container(server)
        container.ssl.server.set_credentials(_testpath('server-certificate-lh.pem'),
                                             _testpath('server-private-key-lh.pem'),
                                             'server-password')
        container.ssl.server.set_trusted_ca_db(_testpath('ca-certificate.pem'))
        container.ssl.server.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                      _testpath('ca-certificate.pem') )

        client = Client()
        config = {
            'scheme':'amqps',
            'port':server.port,
            'sasl': {
                'mechanisms': 'EXTERNAL'
            },
            'tls': {
                'cert': _testpath('client-certificate.pem'),
                'key': _testpath('client-private-key-no-password.pem'),
                'ca': _testpath('ca-certificate.pem'),
                'verify': True
            }
        }
        write_connect_conf(config)
        container.connect(handler=client, reconnect=False)
        container.run()
        assert client.opened == True

    def test_ssl_plain(self):
        ensureCanTestExtendedSASL()
        user = 'user@proton'
        password = 'password'
        server = Server(expected_user=user, scheme='amqps')
        container = Container(server)
        container.ssl.server.set_credentials(_testpath('server-certificate-lh.pem'),
                                             _testpath('server-private-key-lh.pem'),
                                             'server-password')
        container.ssl.server.set_trusted_ca_db(_testpath('ca-certificate.pem'))
        container.ssl.server.set_peer_authentication( SSLDomain.VERIFY_PEER,
                                                      _testpath('ca-certificate.pem') )

        client = Client()
        config = {
            'scheme':'amqps',
            'port':server.port,
            'user':user,
            'password':password,
            'sasl': {
                'mechanisms': 'PLAIN'
             },
            'tls': {
                'cert': _testpath('client-certificate.pem'),
                'key': _testpath('client-private-key-no-password.pem'),
                'ca': _testpath('ca-certificate.pem'),
                'verify': True
            }
        }
        write_connect_conf(config)
        container.connect(handler=client, reconnect=False)
        container.run()
        assert client.opened == True

