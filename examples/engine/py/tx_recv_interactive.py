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

import sys
import threading
from proton.reactors import ApplicationEvent, EventLoop
from proton.handlers import TransactionalClientHandler

class TxRecv(TransactionalClientHandler):
    def __init__(self):
        super(TxRecv, self).__init__(prefetch=0)

    def on_start(self, event):
        self.context = event.reactor.connect("localhost:5672")
        self.receiver = self.context.create_receiver("examples")
        #self.context.declare_transaction(handler=self, settle_before_discharge=False)
        self.context.declare_transaction(handler=self, settle_before_discharge=True)
        self.transaction = None

    def on_message(self, event):
        print event.message.body
        self.transaction.accept(event.delivery)

    def on_transaction_declared(self, event):
        self.transaction = event.transaction
        print "transaction declared"

    def on_transaction_committed(self, event):
        print "transaction committed"
        self.context.declare_transaction(handler=self)

    def on_transaction_aborted(self, event):
        print "transaction aborted"
        self.context.declare_transaction(handler=self)

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
    reactor = EventLoop(TxRecv())
    events = reactor.get_event_trigger()
    thread = threading.Thread(target=reactor.run)
    thread.daemon=True
    thread.start()

    print "Enter 'fetch', 'commit' or 'abort'"
    while True:
        line = sys.stdin.readline()
        if line:
            events.trigger(ApplicationEvent(line.strip()))
        else:
            break
except KeyboardInterrupt: pass


