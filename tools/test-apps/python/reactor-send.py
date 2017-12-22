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
from proton import Message
from proton.handlers import MessagingHandler
from proton.reactor import Container

class Send(MessagingHandler):
    def __init__(self, url, messages, size, replying):
        super(Send, self).__init__(prefetch=1024)
        self.url = url
        self.sent = 0
        self.confirmed = 0
        self.received = 0
        self.received_bytes = 0
        self.total = messages
        self.message_size = size;
        self.replying = replying;
        self.message = Message(body="X" * self.message_size)
        if replying:
            self.message.reply_to = "localhost/test"

    def on_start(self, event):
        event.container.sasl_enabled = False
        event.container.create_sender(self.url)

    def on_sendable(self, event):
        while event.sender.credit and self.sent < self.total:
            self.message.correlation_id = self.sent + 1
            event.sender.send(self.message)
            self.sent += 1

    def on_accepted(self, event):
        self.confirmed += 1
        if self.confirmed == self.total:
            print("all messages confirmed")
            if not self.replying:
                event.connection.close()

    def on_message(self, event):
        msg = event.message;
        if self.received < self.total:
            self.received += 1
            self.received_bytes += len(msg.body)
        if self.received == self.total:
            event.receiver.close()
            event.connection.close()

    def on_disconnected(self, event):
        self.sent = self.confirmed

parser = optparse.OptionParser(usage="usage: %prog [options]",
                               description="Send messages to the supplied address.")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address to which messages are sent (default %default)")
parser.add_option("-c", "--messages", type="int", default=100,
                  help="number of messages to send (default %default)")
parser.add_option("-b", "--bytes", type="int", default=100,
                  help="size of each message body in bytes (default %default)")
parser.add_option("-R", action="store_true", dest="replying", help="process reply messages")

opts, args = parser.parse_args()

try:
    Container(Send(opts.address, opts.messages, opts.bytes, opts.replying)).run()
except KeyboardInterrupt: pass
