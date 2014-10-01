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

import Queue
import time
from proton import Message
from proton_events import ApplicationEvent, BaseHandler, EventLoop
from db_common import Db

class Send(BaseHandler):
    def __init__(self, host, address):
        self.eventloop = EventLoop()
        self.address = address
        self.host = host
        self.delay = 0
        self.sent = 0
        self.records = Queue.Queue(maxsize=50)
        self.db = Db("src_db", self.eventloop.get_event_trigger())
        self.conn = self.eventloop.connect(self.host, handler=self)
        self.sender = self.conn.sender(self.address)

    def on_records_loaded(self, event):
        if self.records.empty() and event.subject == self.sent:
            print "Exhausted available data, waiting to recheck..."
            # check for new data after 5 seconds
            self.eventloop.schedule(time.time() + 5, link=self.sender, subject="data")
        else:
            self.send()

    def request_records(self):
        if not self.records.full():
            self.db.load(self.records, event=ApplicationEvent("records_loaded", link=self.sender, subject=self.sent))

    def on_credit(self, event):
        self.send()

    def send(self):
        while self.sender.credit and not self.records.empty():
            record = self.records.get(False)
            id = record['id']
            self.sender.send_msg(Message(id=id, durable=True, body=record['description']), tag=str(id))
            self.sent += 1
            print "sent message %s" % id
        self.request_records()

    def on_settled(self, event):
        id = int(event.delivery.tag)
        self.db.delete(id)
        print "settled message %s" % id

    def on_disconnected(self, event):
        self.db.reset()

    def on_timer(self, event):
        if event.subject == "data":
            print "Rechecking for data..."
            self.request_records()

    def run(self):
        self.eventloop.run()

try:
    Send("localhost:5672", "examples").run()
except KeyboardInterrupt: pass

