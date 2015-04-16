#!/usr/bin/env python
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

from __future__ import print_function
from proton import Message
from proton.reactor import Container
from proton.handlers import MessagingHandler, TransactionHandler

class TxRequest(TransactionHandler):
    def __init__(self, response, sender, request_delivery):
        super(TxRequest, self).__init__()
        self.response = response
        self.sender = sender
        self.request_delivery = request_delivery

    def on_transaction_declared(self, event):
        event.transaction.send(self.sender, self.response)
        event.transaction.accept(self.request_delivery)
        event.transaction.commit()

    def on_transaction_committed(self, event):
        print("Request processed successfully")

    def on_transaction_aborted(self, event):
        print("Request processing aborted")


class TxServer(MessagingHandler):
    def __init__(self, host, address):
        super(TxServer, self).__init__(auto_accept=False)
        self.host = host
        self.address = address

    def on_start(self, event):
        self.container = event.container
        self.conn = event.container.connect(self.host, reconnect=False)
        self.receiver = event.container.create_receiver(self.conn, self.address)
        self.senders = {}
        self.relay = None

    def on_message(self, event):
        sender = self.relay
        if not sender:
            sender = self.senders.get(event.message.reply_to)
        if not sender:
            sender = self.container.create_sender(self.conn, event.message.reply_to)
            self.senders[event.message.reply_to] = sender

        response = Message(address=event.message.reply_to, body=event.message.body.upper(),
                           correlation_id=event.message.correlation_id)
        self.container.declare_transaction(self.conn, handler=TxRequest(response, sender, event.delivery))

    def on_connection_open(self, event):
        if event.connection.remote_offered_capabilities and 'ANONYMOUS-RELAY' in event.connection.remote_offered_capabilities:
            self.relay = self.container.create_sender(self.conn, None)

try:
    Container(TxServer("localhost:5672", "examples")).run()
except KeyboardInterrupt: pass



