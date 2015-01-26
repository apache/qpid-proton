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
from threading import Thread
from unittest import TestCase
from .common import Test, free_tcp_port
from copy import copy
from proton import Message, Url, generate_uuid
from proton.handlers import MessagingHandler
from proton.reactors import Container, send_msg, delivery_tags
from proton.utils import SyncRequestResponse, BlockingConnection


class EchoServer(MessagingHandler, Thread):
    """
    Simple echo server that echos messages to their reply-to. Runs in a thread.
    Will only accept a single connection and shut down when that connection closes.
    """

    def __init__(self, url):
        MessagingHandler.__init__(self)
        Thread.__init__(self)
        self.url = url
        self.senders = {}
        self.container = None

    def on_start(self, event):
        self.acceptor = event.container.listen(self.url)
        self.container = event.container

    def on_link_opening(self, event):
        if event.link.is_sender:
            if event.link.remote_source and event.link.remote_source.dynamic:
                event.link.source.address = str(generate_uuid())
                self.senders[event.link.source.address] = event.link
                event.link.tags = delivery_tags()

    def on_message(self, event):
        m = event.message
        sender = self.senders.get(m.reply_to)
        if sender:
            reply = Message(address=m.reply_to, body=m.body, correlation_id=m.correlation_id)
            send_msg(sender, reply)

    def on_connection_closing(self, event):
        self.acceptor.close()

    def run(self):
        Container(self).run()


class SyncRequestResponseTest(Test):
    """Test SyncRequestResponse"""

    def test_request_response(self):
        def test(name, address="x"):
            for i in xrange(5):
                body="%s%s" % (name, i)
                response = client.call(Message(address=address, body=body))
                self.assertEquals(response.address, client.reply_to)
                self.assertEquals(response.body, body)

        server = EchoServer(Url(port=free_tcp_port()))
        server.start()
        client = SyncRequestResponse(BlockingConnection(server.url))
        try:
            test("foo")         # Simple request/resposne
        finally:
            client.connection.close()
