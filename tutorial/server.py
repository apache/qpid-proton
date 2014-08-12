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
from proton_events import EventLoop, IncomingMessageHandler

class Server(IncomingMessageHandler):
    def __init__(self, eventloop, host, address):
        self.eventloop = eventloop
        self.conn = eventloop.connect(host)
        self.receiver = self.conn.receiver(address, handler=self)
        self.senders = {}

    def on_message(self, event):
        sender = self.senders.get(event.message.reply_to)
        if not sender:
            sender = self.conn.sender(event.message.reply_to)
            self.senders[event.message.reply_to] = sender
        sender.send_msg(Message(body=event.message.body.upper()))

    def on_connection_remote_close(self, endpoint, error):
        if error: print "Closed due to %s" % error
        self.conn.close()

    def run(self):
        self.eventloop.run()

try:
    Server(EventLoop(), "localhost:5672", "examples").run()
except KeyboardInterrupt: pass



