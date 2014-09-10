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

import time
from proton_events import ApplicationEvent, IncomingMessageHandler, EventLoop, FlowController
from db_common import Db

class Recv(IncomingMessageHandler):
    def __init__(self, host, address):
        self.eventloop = EventLoop()#self, FlowController(10))
        self.host = host
        self.address = address
        self.delay = 0
        self.db = Db("dst_db", self.eventloop.get_event_trigger())
        # TODO: load last tag from db
        self.last_id = None
        self.connect()

    def connect(self):
        self.conn = self.eventloop.connect(self.host, handler=self)

    def auto_accept(self): return False

    def on_record_inserted(self, event):
        self.accept(event.delivery)

    def on_message(self, event):
        id = int(event.message.id)
        if (not self.last_id) or id > self.last_id:
            self.last_id = id
            self.db.insert(id, event.message.body, ApplicationEvent("record_inserted", delivery=event.delivery))
            print "inserted message %s" % id
        else:
            self.accept(event.delivery)

    def on_connection_remote_open(self, event):
        self.delay = 0
        self.conn.receiver(self.address)

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
            self.eventloop.schedule(time.time() + self.delay, connection=conn)
            self.delay = min(10, 2*self.delay)

    def on_timer(self, event):
        print "Reconnecting..."
        self.connect()

    def run(self):
        self.eventloop.run()

try:
    Recv("localhost:5672", "examples").run()
except KeyboardInterrupt: pass



