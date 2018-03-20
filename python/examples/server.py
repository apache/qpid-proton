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
from proton import Message, Url
from proton.handlers import MessagingHandler
from proton.reactor import Container

class Server(MessagingHandler):
    def __init__(self, url, address):
        super(Server, self).__init__()
        self.url = url
        self.address = address

    def on_start(self, event):
        print("Listening on", self.url)
        self.container = event.container
        self.conn = event.container.connect(self.url)
        self.receiver = event.container.create_receiver(self.conn, self.address)
        self.server = self.container.create_sender(self.conn, None)

    def on_message(self, event):
        print("Received", event.message)
        self.server.send(Message(address=event.message.reply_to, body=event.message.body.upper(),
                            correlation_id=event.message.correlation_id))

parser = optparse.OptionParser(usage="usage: %prog [options]")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address from which messages are received (default %default)")
opts, args = parser.parse_args()

url = Url(opts.address)

try:
    Container(Server(url, url.path)).run()
except KeyboardInterrupt: pass



