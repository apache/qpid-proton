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

from proton_events import ApplicationEvent, BaseHandler, EventLoop
from db_common import Db

class Recv(BaseHandler):
    def __init__(self, host, address):
        self.eventloop = EventLoop()
        self.host = host
        self.address = address
        self.delay = 0
        self.db = Db("dst_db", self.eventloop.get_event_trigger())
        # TODO: load last tag from db
        self.last_id = None
        self.conn = self.eventloop.connect(self.host, handler=self)
        self.conn.create_receiver(self.address)

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

    def run(self):
        self.eventloop.run()

try:
    Recv("localhost:5672", "examples").run()
except KeyboardInterrupt: pass



