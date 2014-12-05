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

from proton.reactors import Container
from proton.handlers import TransactionalClientHandler

class TxRecv(TransactionalClientHandler):
    def __init__(self, batch_size):
        super(TxRecv, self).__init__(prefetch=0)
        self.current_batch = 0
        self.batch_size = batch_size

    def on_start(self, event):
        self.container = event.container
        self.conn = self.container.connect("localhost:5672")
        self.receiver = self.container.create_receiver(self.conn, "examples")
        self.container.declare_transaction(self.conn, handler=self)
        self.transaction = None

    def on_message(self, event):
        print event.message.body
        self.accept(event.delivery, self.transaction)
        self.current_batch += 1
        if self.current_batch == self.batch_size:
            self.transaction.commit()
            self.transaction = None

    def on_transaction_declared(self, event):
        self.receiver.flow(self.batch_size)
        self.transaction = event.transaction

    def on_transaction_committed(self, event):
        self.current_batch = 0
        self.container.declare_transaction(self.conn, handler=self)

    def on_disconnected(self, event):
        self.current_batch = 0

try:
    Container(TxRecv(10)).run()
except KeyboardInterrupt: pass



