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
import optparse
from proton.handlers import MessagingHandler
from proton.reactor import ApplicationEvent, Container, EventInjector
from db_common import Db

class Recv(MessagingHandler):
    def __init__(self, url, count):
        super(Recv, self).__init__(auto_accept=False)
        self.url = url
        self.delay = 0
        self.last_id = None
        self.expected = count
        self.received = 0
        self.accepted = 0
        self.db = Db("dst_db", EventInjector())

    def on_start(self, event):
        event.container.selectable(self.db.injector)
        e = ApplicationEvent("id_loaded")
        e.container = event.container
        self.db.get_id(e)

    def on_id_loaded(self, event):
        self.last_id = event.id
        event.container.create_receiver(self.url)

    def on_record_inserted(self, event):
        self.accept(event.delivery)
        self.accepted += 1
        if self.accepted == self.expected:
            event.connection.close()
            self.db.close()

    def on_message(self, event):
        id = int(event.message.id)
        if (not self.last_id) or id > self.last_id:
            if self.received < self.expected:
                self.received += 1
                self.last_id = id
                self.db.insert(id, event.message.body, ApplicationEvent("record_inserted", delivery=event.delivery))
                print("inserted message %s" % id)
            else:
                self.release(event.delivery)
        else:
            self.accept(event.delivery)

parser = optparse.OptionParser(usage="usage: %prog [options]")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address from which messages are received (default %default)")
parser.add_option("-m", "--messages", type="int", default=0,
                  help="number of messages to receive; 0 receives indefinitely (default %default)")
opts, args = parser.parse_args()

try:
    Container(Recv(opts.address, opts.messages)).run()
except KeyboardInterrupt: pass



