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
import sys
import threading
from proton.reactor import ApplicationEvent, Container
from proton.handlers import MessagingHandler, TransactionHandler

class TxRecv(MessagingHandler, TransactionHandler):
    def __init__(self):
        super(TxRecv, self).__init__(prefetch=0, auto_accept=False)

    def on_start(self, event):
        self.container = event.container
        self.conn = self.container.connect("localhost:5672")
        self.receiver = self.container.create_receiver(self.conn, "examples")
        self.container.declare_transaction(self.conn, handler=self, settle_before_discharge=True)
        self.transaction = None

    def on_message(self, event):
        print(event.message.body)
        self.transaction.accept(event.delivery)

    def on_transaction_declared(self, event):
        self.transaction = event.transaction
        print("transaction declared")

    def on_transaction_committed(self, event):
        print("transaction committed")
        self.container.declare_transaction(self.conn, handler=self)

    def on_transaction_aborted(self, event):
        print("transaction aborted")
        self.container.declare_transaction(self.conn, handler=self)

    def on_commit(self, event):
        self.transaction.commit()

    def on_abort(self, event):
        self.transaction.abort()

    def on_fetch(self, event):
        self.receiver.flow(1)

    def on_quit(self, event):
        c = self.receiver.connection
        self.receiver.close()
        c.close()

try:
    reactor = Container(TxRecv())
    events = reactor.get_event_trigger()
    thread = threading.Thread(target=reactor.run)
    thread.daemon=True
    thread.start()

    print("Enter 'fetch', 'commit' or 'abort'")
    while True:
        line = sys.stdin.readline()
        if line:
            events.trigger(ApplicationEvent(line.strip()))
        else:
            break
except KeyboardInterrupt: pass


