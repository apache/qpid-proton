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
from proton_events import ApplicationEvent, EventLoop, OutgoingMessageHandler
from db_common import Db

class Send(OutgoingMessageHandler):
    def __init__(self, host, address):
        self.eventloop = EventLoop()
        self.address = address
        self.host = host
        self.delay = 0
        self.sent = 0
        self.records = Queue.Queue(maxsize=50)
        self.db = Db("src_db", self.eventloop.get_event_trigger())
        self.connect()

    def connect(self):
        self.conn = self.eventloop.connect(self.host, handler=self)

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

    def on_link_flow(self, event):
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

    def on_connection_remote_open(self, event):
        self.db.reset()
        self.sender = self.conn.sender(self.address)
        self.delay = 0

    def on_link_remote_close(self, event):
        self.closed(event.link.remote_condition)

    def on_connection_remote_close(self, event):
        self.closed(event.connection.remote_condition)

    def closed(self, error=None):
        if error:
            print "Closed due to %s" % error
        self.conn.close()

    def on_disconnected(self, conn):
        if self.delay == 0:
            self.delay = 0.1
            print "Disconnected, reconnecting..."
            self.connect()
        else:
            print "Disconnected will try to reconnect after %d seconds" % self.delay
            self.eventloop.schedule(time.time() + self.delay, connection=conn, subject="reconnect")
            self.delay = min(10, 2*self.delay)

    def on_timer(self, event):
        if event.subject == "reconnect":
            print "Reconnecting..."
            self.connect()
        elif event.subject == "data":
            print "Rechecking for data..."
            self.request_records()

    def run(self):
        self.eventloop.run()

try:
    Send("localhost:5672", "examples").run()
except KeyboardInterrupt: pass

