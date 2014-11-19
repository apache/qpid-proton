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

from proton import Message
from proton_handlers import MessagingHandler
from proton_reactors import EventLoop

class Send(MessagingHandler):
    def __init__(self, host, address, messages):
        super(Send, self).__init__()
        self.host = host
        self.address = address
        self.sent = 0
        self.confirmed = 0
        self.total = messages

    def on_start(self, event):
        conn = event.reactor.connect(self.host)
        conn.create_sender(self.address)

    def on_credit(self, event):
        while event.sender.credit and self.sent < self.total:
            msg = Message(body={'sequence':(self.sent+1)})
            event.sender.send_msg(msg)
            self.sent += 1

    def on_accepted(self, event):
        self.confirmed += 1
        if self.confirmed == self.total:
            print "all messages confirmed"
            event.connection.close()

    def on_disconnected(self, event):
        self.sent = self.confirmed

try:
    EventLoop(Send("localhost:5672", "examples", 10000)).run()
except KeyboardInterrupt: pass
