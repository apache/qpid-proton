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
import platform

import gc
import os
import socket
import threading

import cproton

import proton.handlers
import proton.utils
import proton.reactor

from test_unittest import unittest


class Broker(proton.handlers.MessagingHandler):
    def __init__(self, acceptor_url, ssl_domain=None):
        # type: (str, proton.SSLDomain) -> None
        super(Broker, self).__init__()
        self.acceptor_url = acceptor_url
        self.ssl_domain = ssl_domain

        self.acceptor = None
        self._acceptor_opened_event = threading.Event()

        self.on_message_ = threading.Event()

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
    def __init__(self, msg_id, urls, ssl_domain=None, *args, **kwargs):
        # type: (str, str, proton.SSLDomain, *object, **object) -> None
        super(SampleSender, self).__init__(*args, **kwargs)
        self.urls = urls
        self.msg_id = msg_id
        self.ssl_domain = ssl_domain

    def on_start(self, event):
        # type: (proton.Event) -> None
        conn = event.container.connect(url=self.urls, reconnect=False, ssl_domain=self.ssl_domain)
        event.container.create_sender(conn, target='topic://VirtualTopic.event')

    def on_sendable(self, event):
        msg = proton.Message(body={'msg-id': self.msg_id, 'name': 'python'})
        event.sender.send(msg)
        event.sender.close()
        event.connection.close()

    def on_connection_error(self, event):
        print("on_error", event)


class Proton2111Test(unittest.TestCase):
    @unittest.skipIf(platform.system() == 'Windows', "TODO jdanek: Test is broken on Windows")
    def test_send_message_ssl_no_object_leaks(self):
        """Starts a broker with ssl acceptor, in a loop connects to it and sends message.

        The test checks that number of Python objects is not increasing inside the loop.
        """
        cwd = os.path.dirname(__file__)
        cert_file = os.path.join(cwd, 'certificates', 'localhost_ca1.pem')
        key_file = os.path.join(cwd, 'certificates', 'localhost_ca1-key.pem')
        certificate_db = os.path.join(cwd, 'certificates', 'ca1.pem')
        password = None

        broker_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_SERVER)
        broker_ssl_domain.set_credentials(cert_file, key_file, password=password)

        client_ssl_domain = proton.SSLDomain(proton.SSLDomain.MODE_CLIENT)
        client_ssl_domain.set_trusted_ca_db(certificate_db)
        client_ssl_domain.set_peer_authentication(proton.SSLDomain.VERIFY_PEER)

        def send_msg(msg_id, urls):
            container = proton.reactor.Container(SampleSender(msg_id, urls, client_ssl_domain))
            container.run()

        with test_broker(ssl_domain=broker_ssl_domain) as broker:
            urls = "amqps://{0}:{1}".format(*broker.get_acceptor_sockname())

            gc.collect()
            object_counts = []
            for i in range(300):
                send_msg(i + 1, urls)
                broker.on_message_.wait()  # message got through
                broker.on_message_.clear()
                gc.collect()
                object_counts.append(len(gc.get_objects()))

        # drop first few values, it is usually different (before counts settle)
        object_counts = object_counts[2:]

        diffs = [c - object_counts[0] for c in object_counts]
        for diff in diffs:
            # allow for random variation from initial value on some systems, but prohibit linear growth
            self.assertTrue(diff <= 50, "Object counts should not be increasing: {0}".format(diffs))
