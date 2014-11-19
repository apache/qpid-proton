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

from proton import Message
from proton_reactors import EventLoop
from proton_handlers import MessagingHandler, TransactionHandler

class TxRequest(TransactionHandler):
    def __init__(self, response, sender, request_delivery, context):
        super(TxRequest, self).__init__()
        self.response = response
        self.sender = sender
        self.request_delivery = request_delivery
        self.context = context

    def on_transaction_declared(self, event):
        self.sender.send_msg(self.response, transaction=event.transaction)
        self.accept(self.request_delivery, transaction=event.transaction)
        event.transaction.commit()

    def on_transaction_committed(self, event):
        print "Request processed successfully"

    def on_transaction_aborted(self, event):
        print "Request processing aborted"


class TxServer(MessagingHandler):
    def __init__(self, host, address):
        super(TxServer, self).__init__(auto_accept=False)
        self.host = host
        self.address = address

    def on_start(self, event):
        self.context = event.reactor.connect(self.host, reconnect=False)
        self.receiver = self.context.create_receiver(self.address)
        self.senders = {}
        self.relay = None

    def on_message(self, event):
        sender = self.relay
        if not sender:
            sender = self.senders.get(event.message.reply_to)
        if not sender:
            sender = self.context.create_sender(event.message.reply_to)
            self.senders[event.message.reply_to] = sender

        response = Message(address=event.message.reply_to, body=event.message.body.upper())
        self.context.declare_transaction(handler=TxRequest(response, sender, event.delivery, self.context))

    def on_connection_open(self, event):
        if event.connection.remote_offered_capabilities and 'ANONYMOUS-RELAY' in event.connection.remote_offered_capabilities:
            self.relay = self.context.create_sender(None)

try:
    EventLoop(TxServer("localhost:5672", "examples")).run()
except KeyboardInterrupt: pass



