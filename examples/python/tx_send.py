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

from __future__ import print_function, unicode_literals
import optparse
from proton import Message, Url
from proton.reactor import Container
from proton.handlers import MessagingHandler, TransactionHandler

class TxSend(MessagingHandler, TransactionHandler):
    def __init__(self, url, messages, batch_size):
        super(TxSend, self).__init__()
        self.url = Url(url)
        self.current_batch = 0
        self.committed = 0
        self.confirmed = 0
        self.total = messages
        self.batch_size = batch_size

    def on_start(self, event):
        self.container = event.container
        self.conn = self.container.connect(self.url)
        self.sender = self.container.create_sender(self.conn, self.url.path)
        self.container.declare_transaction(self.conn, handler=self)
        self.transaction = None

    def on_transaction_declared(self, event):
        self.transaction = event.transaction
        self.send()

    def on_sendable(self, event):
        self.send()

    def send(self):
        while self.transaction and self.sender.credit and (self.committed + self.current_batch) < self.total:
            seq = self.committed + self.current_batch + 1
            msg = Message(id=seq, body={'sequence':seq})
            self.transaction.send(self.sender, msg)
            self.current_batch += 1
            if self.current_batch == self.batch_size:
                self.transaction.commit()
                self.transaction = None

    def on_accepted(self, event):
        if event.sender == self.sender:
            self.confirmed += 1

    def on_transaction_committed(self, event):
        self.committed += self.current_batch
        if self.committed == self.total:
            print("all messages committed")
            event.connection.close()
        else:
            self.current_batch = 0
            self.container.declare_transaction(self.conn, handler=self)

    def on_disconnected(self, event):
        self.current_batch = 0

parser = optparse.OptionParser(usage="usage: %prog [options]",
                               description="Send messages transactionally to the supplied address.")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address to which messages are sent (default %default)")
parser.add_option("-m", "--messages", type="int", default=100,
                  help="number of messages to send (default %default)")
parser.add_option("-b", "--batch-size", type="int", default=10,
                  help="number of messages in each transaction (default %default)")
opts, args = parser.parse_args()

try:
    Container(TxSend(opts.address, opts.messages, opts.batch_size)).run()
except KeyboardInterrupt: pass
