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
# under the License
#

"""
PROTON-2111 python: memory leak on Container, SSL, and SSLDomain objects
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import logging

import os
try:
    import queue
except ImportError:
    import Queue as queue  # for Python 2.6, 2.7 compatibility
import socket
import ssl
import threading

import cproton

import proton.handlers
import proton.utils
import proton.reactor

from test_unittest import unittest


class Broker(proton.handlers.MessagingHandler):
    """Mock broker with TLS support and error capture."""
    def __init__(self, acceptor_url, ssl_domain=None):
        # type: (str, proton.SSLDomain) -> None
        super(Broker, self).__init__()
        self.acceptor_url = acceptor_url
        self.ssl_domain = ssl_domain

        self.acceptor = None
        self._acceptor_opened_event = threading.Event()

        self.on_message_ = threading.Event()
        self.errors = []

    def get_acceptor_sockname(self):
        # type: () -> (str, int)
        self._acceptor_opened_event.wait()
        if hasattr(self.acceptor, '_selectable'):  # proton 0.30.0+
            sockname = self.acceptor._selectable._delegate.getsockname()
        else:  # works in proton 0.27.0
            selectable = cproton.pn_cast_pn_selectable(self.acceptor._impl)
            fd = cproton.pn_selectable_get_fd(selectable)
            s = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
            sockname = s.getsockname()
        return sockname[:2]

    def on_start(self, event):
        self.acceptor = event.container.listen(self.acceptor_url, ssl_domain=self.ssl_domain)
        self._acceptor_opened_event.set()

    def on_link_opening(self, event):
        link = event.link  # type: proton.Link
        if link.is_sender:
            assert not link.remote_source.dynamic, "This cannot happen"
            link.source.address = link.remote_source.address
        elif link.remote_target.address:
            link.target.address = link.remote_target.address

    def on_message(self, event):
        self.on_message_.set()

    def on_transport_error(self, event):
        super(Broker, self).on_transport_error(event)
        self.errors.append(event.transport.condition)


@contextlib.contextmanager
def test_broker(ssl_domain=None):
    # type: (proton.SSLDomain) -> Broker
    broker = Broker('localhost:0', ssl_domain=ssl_domain)
    container = proton.reactor.Container(broker)
    t = threading.Thread(target=container.run)
    t.start()

    yield broker

    container.stop()
    if broker.acceptor:
        broker.acceptor.close()
    t.join()


class SampleSender(proton.handlers.MessagingHandler):
    """Client with TLS support which sends one message and then ends."""
    def __init__(self, msg_id, urls, ssl_domain=None, *args, **kwargs):
        # type: (str, str, proton.SSLDomain, *object, **object) -> None
        super(SampleSender, self).__init__(*args, **kwargs)
        self.urls = urls
        self.msg_id = msg_id
        self.ssl_domain = ssl_domain

        self.errors = []

    def on_start(self, event):
        # type: (proton.Event) -> None
        conn = event.container.connect(url=self.urls, reconnect=False, ssl_domain=self.ssl_domain)
        event.container.create_sender(conn, target='someTarget')

    def on_sendable(self, event):
        msg = proton.Message(body={'msg-id': self.msg_id, 'name': 'python'})
        event.sender.send(msg)
        event.sender.close()
        event.connection.close()

    def on_transport_error(self, event):
        super(SampleSender, self).on_transport_error(event)
        self.errors.append(event.transport.condition)


class Proton1870Test(unittest.TestCase):
    """Starts a broker with ssl configuration (or without it, in some cases) and connects
    to it with a client to check if helpful error messages are logged."""
    cwd = os.path.dirname(__file__)

    def test_broker_cert_success(self):
        """Basic TLS scenario without any error."""
        certificate_db = os.path.join(self.cwd, 'certificates', 'ca1.pem')

        cert_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1.pem')
        key_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1-key.pem')

        broker_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_SERVER)
        broker_ssl_domain.set_credentials(cert_file, key_file, password=None)

        client_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_CLIENT)
        client_ssl_domain.set_trusted_ca_db(certificate_db)
        client_ssl_domain.set_peer_authentication(proton.SSLDomain.VERIFY_PEER)

        with test_broker(ssl_domain=broker_ssl_domain) as broker:
            urls = "amqps://localhost:{0}".format(broker.get_acceptor_sockname()[1])
            container = proton.reactor.Container(SampleSender('msg_id', urls, client_ssl_domain))
            container.run()

    def test_broker_cert_shutdown_connection_sslsock(self):
        """When a remote peer drops TCP connection (with established
         SSL+AMQP connection.session on it) and it drops the TCP
         connection by sending FIN+ACK packet, descriptive error is generated."""
        port_q = queue.Queue()

        def server():
            """Mock TLS server without any AMQP support which kills incoming connections."""
            cert_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1.pem')
            key_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1-key.pem')

            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(cert_file, key_file)

            with socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0) as sock:
                sock.bind(('localhost', 0))
                sock.listen(5)
                with context.wrap_socket(sock, server_side=True) as ssock:
                    port_q.put(ssock.getsockname()[1])
                    conn, _ = ssock.accept()
                    conn.shutdown(socket.SHUT_RDWR)
                    conn.close()

        t = threading.Thread(target=server)
        t.start()

        try:
            url = "amqps://localhost:{0}".format(port_q.get())
            bc = proton.utils.BlockingConnection(url)
            s = bc.create_sender("address")
            s.send(proton.Message())
            self.fail("Expected a ConnectionException")
        except proton.ConnectionException as e:
            # TODO XXX "SSL Failure: Unknown error" string is misleading, as TLS was closed cleanly
            error_message = str(e)
            self.assertIn("amqp:connection:framing-error", error_message)
            self.assertIn("SSL Failure: Unknown error", error_message)

        t.join()

    def test_broker_cert_file_does_not_exist(self):
        """When the certificate files we specified do not exist
        on disk, we get an error."""
        cert_file = os.path.join(self.cwd, 'certificates', 'no_such_file.pem')
        key_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1-key.pem')

        broker_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_SERVER)
        try:
            broker_ssl_domain.set_credentials(cert_file, key_file, password=None)
        except proton.SSLException as e:
            # TODO XXX "SSL failure" is too generic, it should talk about missing files instead
            error_message = str(e)
            self.assertIn("SSL failure", error_message)

    def test_brokers_ca_not_trusted_by_client(self):
        cert_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1.pem')
        key_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1-key.pem')

        broker_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_SERVER)
        broker_ssl_domain.set_credentials(cert_file, key_file, password=None)

        # intentionally not setting trusted_ca_db here
        client_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_CLIENT)
        client_ssl_domain.set_peer_authentication(proton.SSLDomain.VERIFY_PEER)

        with test_broker(ssl_domain=broker_ssl_domain) as broker:
            urls = "amqps://localhost:{0}".format(broker.get_acceptor_sockname()[1])
            sender = SampleSender('msg_id', urls, client_ssl_domain)
            container = proton.reactor.Container(sender)
            container.run()
            # TODO XXX "certificate verify failed" is too generic,
            #  it should say exactly what is wrong with the certificate, e.g. wrong hostname, expired certificate, ...
            self.assertEqual(1, len(sender.errors))
            client_error = str(sender.errors[0])
            self.assertIn("amqp:connection:framing-error", client_error)
            self.assertIn("certificate verify failed", client_error)
        # TODO XXX "Unknown error" is unhelpful in diagnosing the problem
        self.assertEqual(1, len(broker.errors))
        broker_error = str(broker.errors[0])
        self.assertIn("amqp:connection:framing-error", broker_error)
        self.assertIn("SSL Failure: Unknown error", broker_error)

    def test_broker_certificate_fails_peer_name_check(self):
        cert_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1.pem')
        key_file = os.path.join(self.cwd, 'certificates', 'localhost_ca1-key.pem')
        certificate_db = os.path.join(self.cwd, 'certificates', 'ca1.pem')

        broker_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_SERVER)
        broker_ssl_domain.set_credentials(cert_file, key_file, password=None)

        client_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_CLIENT)
        client_ssl_domain.set_trusted_ca_db(certificate_db)
        client_ssl_domain.set_peer_authentication(proton.SSLDomain.VERIFY_PEER_NAME)

        with test_broker(ssl_domain=broker_ssl_domain) as broker:
            urls = "amqps://127.0.0.1:{0}".format(broker.get_acceptor_sockname()[1])
            sender = SampleSender('msg_id', urls, client_ssl_domain)
            container = proton.reactor.Container(sender)
            container.run()
            # TODO XXX "certificate verify failed" is too generic,
            #  it should say exactly what is wrong with the certificate, e.g. wrong hostname, expired certificate, ...
            self.assertEqual(1, len(sender.errors))
            client_error = str(sender.errors[0])
            self.assertIn("amqp:connection:framing-error", client_error)
            self.assertIn("certificate verify failed", client_error)
        # TODO XXX "Unknown error" is unhelpful in diagnosing the problem
        self.assertEqual(1, len(broker.errors))
        broker_error = str(broker.errors[0])
        self.assertIn("amqp:connection:framing-error", broker_error)
        self.assertIn("SSL Failure: Unknown error", broker_error)
