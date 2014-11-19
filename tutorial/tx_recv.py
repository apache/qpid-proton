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

import proton_events

class TxRecv(proton_events.TransactionalClientHandler):
    def __init__(self, batch_size):
        super(TxRecv, self).__init__(prefetch=0)
        self.current_batch = 0
        self.batch_size = batch_size
        self.event_loop = proton_events.EventLoop(self)
        self.conn = self.event_loop.connect("localhost:5672")
        self.receiver = self.conn.create_receiver("examples")
        self.conn.declare_transaction(handler=self)
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
        self.conn.declare_transaction(handler=self)

    def on_disconnected(self, event):
        self.current_batch = 0

    def run(self):
        self.event_loop.run()

try:
    TxRecv(10).run()
except KeyboardInterrupt: pass



