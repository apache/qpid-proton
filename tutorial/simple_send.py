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
from proton import Message
from proton_events import OutgoingMessageHandler, EventLoop

class Send(OutgoingMessageHandler):
    def __init__(self, eventloop, host, address, messages):
        self.eventloop = eventloop
        self.sent = 0
        self.confirmed = 0
        self.total = messages
        self.address = address
        self.host = host
        self.delay = 0
        self.connect()

    def connect(self):
        self.conn = self.eventloop.connect(self.host, handler=self)

    def on_link_flow(self, event):
        for i in range(self.sender.credit):
            if self.sent == self.total:
                self.sender.drained()
                break
            msg = Message(body={'sequence':self.sent})
            self.sender.send_msg(msg, handler=self)
            self.sent += 1

    def on_accepted(self, event):
        """
        Stop the application once all of the messages are sent and acknowledged,
        """
        self.confirmed += 1
        if self.confirmed == self.total:
            self.sender.close()
            self.conn.close()

    def on_connection_remote_open(self, event):
        self.sent = self.confirmed
        self.sender = self.conn.sender(self.address)
        self.sender.offered(self.total)
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
            self.eventloop.schedule(time.time() + self.delay, connection=conn)
            self.delay = min(10, 2*self.delay)

    def on_timer(self, event):
        print "Reconnecting..."
        self.connect()

    def run(self):
        self.eventloop.run()

Send(EventLoop(), "localhost:5672", "examples", 10000).run()

