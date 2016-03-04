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

import os, time, sys
from threading import Thread, Event
from unittest import TestCase
from proton_tests.common import Test, free_tcp_port
from copy import copy
from proton import Message, Url, generate_uuid, Array, UNDESCRIBED, Data, symbol, ConnectionException
from proton.handlers import MessagingHandler
from proton.reactor import Container
from proton.utils import SyncRequestResponse, BlockingConnection
from .common import Skipped
CONNECTION_PROPERTIES={u'connection': u'properties'}
OFFERED_CAPABILITIES = Array(UNDESCRIBED, Data.SYMBOL, symbol("O_one"), symbol("O_two"), symbol("O_three"))
DESIRED_CAPABILITIES = Array(UNDESCRIBED, Data.SYMBOL, symbol("D_one"), symbol("D_two"), symbol("D_three"))
ANONYMOUS='ANONYMOUS'
EXTERNAL='EXTERNAL'

class EchoServer(MessagingHandler, Thread):
    """
    Simple echo server that echos messages to their reply-to. Runs in a thread.
    Will only accept a single connection and shut down when that connection closes.
    """

    def __init__(self, url, timeout):
        MessagingHandler.__init__(self)
        Thread.__init__(self)
        self.daemon = True
        self.timeout = timeout
        self.url = url
        self.senders = {}
        self.container = None
        self.event = Event()

    def on_start(self, event):
        self.acceptor = event.container.listen(self.url)
        self.container = event.container
        self.event.set()
        
    def on_link_opening(self, event):
        if event.link.is_sender:
            if event.link.remote_source and event.link.remote_source.dynamic:
                event.link.source.address = str(generate_uuid())
                self.senders[event.link.source.address] = event.link

    def on_message(self, event):
        m = event.message
        sender = self.senders.get(m.reply_to)
        if sender:
            reply = Message(address=m.reply_to, body=m.body, correlation_id=m.correlation_id)
            sender.send(reply)

    def on_connection_closing(self, event):
        self.acceptor.close()

    def run(self):
        Container(self).run()

    def wait(self):
        self.event.wait(self.timeout)


class ConnPropertiesServer(EchoServer):
     def __init__(self, url, timeout):
        EchoServer.__init__(self, url, timeout)
        self.properties_received = False
        self.offered_capabilities_received = False
        self.desired_capabilities_received = False

     def on_connection_opening(self, event):
        conn = event.connection
                   
        if conn.remote_properties == CONNECTION_PROPERTIES:
            self.properties_received = True
        if conn.remote_offered_capabilities == OFFERED_CAPABILITIES:
            self.offered_capabilities_received = True
        if conn.remote_desired_capabilities == DESIRED_CAPABILITIES:
            self.desired_capabilities_received = True
        
class SyncRequestResponseTest(Test):
    """Test SyncRequestResponse"""

    def test_request_response(self):
        def test(name, address="x"):
            for i in range(5):
                body="%s%s" % (name, i)
                response = client.call(Message(address=address, body=body))
                self.assertEquals(response.address, client.reply_to)
                self.assertEquals(response.body, body)

        server = EchoServer(Url(host="127.0.0.1", port=free_tcp_port()), self.timeout)
        server.start()
        server.wait()
        connection = BlockingConnection(server.url, timeout=self.timeout)
        client = SyncRequestResponse(connection)
        try:
            test("foo")         # Simple request/resposne
        finally:
            client.connection.close()
        server.join(timeout=self.timeout)


    def test_connection_properties(self):
        server = ConnPropertiesServer(Url(host="127.0.0.1", port=free_tcp_port()), timeout=self.timeout)
        server.start()
        server.wait()
        connection = BlockingConnection(server.url, timeout=self.timeout, properties=CONNECTION_PROPERTIES, offered_capabilities=OFFERED_CAPABILITIES, desired_capabilities=DESIRED_CAPABILITIES)
        client = SyncRequestResponse(connection)
        client.connection.close()
        server.join(timeout=self.timeout)
        self.assertEquals(server.properties_received, True)
        self.assertEquals(server.offered_capabilities_received, True)
        self.assertEquals(server.desired_capabilities_received, True)

    def test_allowed_mechs_external(self):
        # All this test does it make sure that if we pass allowed_mechs to BlockingConnection, it is actually used. 
        if "java" in sys.platform:
            raise Skipped("")
        port = free_tcp_port()
        # We have constructed ConnPropertiesServer with host="127.0.0.1", so it is ok to hardcode the hostname in the error message string.
        error_message = 'Connection amqp://127.0.0.1:' + str(port) + ' disconnected'
        server = ConnPropertiesServer(Url(host="127.0.0.1", port=port), timeout=self.timeout)
        server.start()
        server.wait()
        exception_occurred = False
        try:
            # This will cause an exception because we are specifying allowed_mechs as EXTERNAL. The SASL handshake will fail because the server is not setup to handle EXTERNAL
            connection = BlockingConnection(server.url, timeout=self.timeout, properties=CONNECTION_PROPERTIES, offered_capabilities=OFFERED_CAPABILITIES, desired_capabilities=DESIRED_CAPABILITIES, allowed_mechs=EXTERNAL)
        except ConnectionException as e:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            self.assertEquals(error_message, str(exc_value))
            exception_occurred = True

        self.assertEquals(True, exception_occurred)

        server.join(timeout=self.timeout)

    def test_allowed_mechs_anonymous(self):
        # All this test does it make sure that if we pass allowed_mechs to BlockingConnection, it is actually used.
        server = ConnPropertiesServer(Url(host="127.0.0.1", port=free_tcp_port()), timeout=self.timeout)
        server.start()
        server.wait()
        # An ANONYMOUS allowed_mechs will work, anonymous connections are allowed by ConnPropertiesServer
        connection = BlockingConnection(server.url, timeout=self.timeout, properties=CONNECTION_PROPERTIES, offered_capabilities=OFFERED_CAPABILITIES, desired_capabilities=DESIRED_CAPABILITIES, allowed_mechs=ANONYMOUS)
        client = SyncRequestResponse(connection)
        client.connection.close()
        server.join(timeout=self.timeout)
        self.assertEquals(server.properties_received, True)
        self.assertEquals(server.offered_capabilities_received, True)
        self.assertEquals(server.desired_capabilities_received, True)
        
        

