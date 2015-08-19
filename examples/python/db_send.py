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
import time
try:
    import Queue
except:
    import queue as Queue


from proton import Message
from proton.handlers import MessagingHandler
from proton.reactor import ApplicationEvent, Container, EventInjector
from db_common import Db

class Send(MessagingHandler):
    def __init__(self, url, count):
        super(Send, self).__init__()
        self.url = url
        self.delay = 0
        self.sent = 0
        self.confirmed = 0
        self.load_count = 0
        self.records = Queue.Queue(maxsize=50)
        self.target = count
        self.db = Db("src_db", EventInjector())

    def keep_sending(self):
        return self.target == 0 or self.sent < self.target

    def on_start(self, event):
        self.container = event.container
        self.container.selectable(self.db.injector)
        self.sender = self.container.create_sender(self.url)

    def on_records_loaded(self, event):
        if self.records.empty():
            if event.subject == self.load_count:
                print("Exhausted available data, waiting to recheck...")
                # check for new data after 5 seconds
                self.container.schedule(5, self)
        else:
            self.send()

    def request_records(self):
        if not self.records.full():
            print("loading records...")
            self.load_count += 1
            self.db.load(self.records, event=ApplicationEvent("records_loaded", link=self.sender, subject=self.load_count))

    def on_sendable(self, event):
        self.send()

    def send(self):
        while self.sender.credit and not self.records.empty():
            if not self.keep_sending(): return
            record = self.records.get(False)
            id = record['id']
            self.sender.send(Message(id=id, durable=True, body=record['description']), tag=str(id))
            self.sent += 1
            print("sent message %s" % id)
        self.request_records()

    def on_settled(self, event):
        id = int(event.delivery.tag)
        self.db.delete(id)
        print("settled message %s" % id)
        self.confirmed += 1
        if self.confirmed == self.target:
            event.connection.close()
            self.db.close()

    def on_disconnected(self, event):
        self.db.reset()
        self.sent = self.confirmed

    def on_timer_task(self, event):
        print("Rechecking for data...")
        self.request_records()

parser = optparse.OptionParser(usage="usage: %prog [options]",
                               description="Send messages to the supplied address.")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address to which messages are sent (default %default)")
parser.add_option("-m", "--messages", type="int", default=0,
                  help="number of messages to send; 0 sends indefinitely (default %default)")
opts, args = parser.parse_args()

try:
    Container(Send(opts.address, opts.messages)).run()
except KeyboardInterrupt: pass

