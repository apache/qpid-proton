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
from proton.reactors import Container
from proton.handlers import TransactionalClientHandler

class TxSend(TransactionalClientHandler):
    def __init__(self, messages, batch_size):
        super(TxSend, self).__init__()
        self.current_batch = 0
        self.committed = 0
        self.confirmed = 0
        self.total = messages
        self.batch_size = batch_size

    def on_start(self, event):
        self.container = event.container
        self.conn = self.container.connect("localhost:5672", handler=self)
        self.sender = self.container.create_sender(self.conn, "examples")
        self.container.declare_transaction(self.conn, handler=self)
        self.transaction = None

    def on_transaction_declared(self, event):
        self.transaction = event.transaction
        self.send()

    def on_credit(self, event):
        self.send()

    def send(self):
        while self.transaction and self.current_batch < self.batch_size and self.sender.credit and self.committed < self.total:
            msg = Message(body={'sequence':(self.committed+self.current_batch+1)})
            self.sender.send_msg(msg, transaction=self.transaction)
            self.current_batch += 1

    def on_accepted(self, event):
        if event.sender == self.sender:
            self.confirmed += 1
            if self.confirmed == self.batch_size:
                self.transaction.commit()
                self.transaction = None
                self.confirmed = 0

    def on_transaction_committed(self, event):
        self.committed += self.current_batch
        if self.committed == self.total:
            print "all messages committed"
            event.connection.close()
        else:
            self.current_batch = 0
            self.container.declare_transaction(self.conn, handler=self)

    def on_disconnected(self, event):
        self.current_batch = 0

try:
    Container(TxSend(10000, 10)).run()
except KeyboardInterrupt: pass
