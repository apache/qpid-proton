#!/usr/bin/env python3
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

import optparse
from proton.handlers import MessagingHandler
from proton.reactor import Container


class Recv(MessagingHandler):
    def __init__(self, url, count):
        super(Recv, self).__init__()
        self.url = url
        self.expected = count
        self.received = 0
        self.dedup_ids = set()

    def on_start(self, event):
        event.container.create_receiver(self.url)

    def on_message(self, event):
        message = event.message
        if message.id in self.dedup_ids:
            # ignore duplicate message
            print(f"duplicate message ignored {message.id}")
            return
        self.dedup_ids.add(message.id)
        if self.expected == 0 or self.received < self.expected:
            print(f"{message.body}")
            self.received += 1
            if self.received == self.expected:
                event.receiver.close()
                event.connection.close()


parser = optparse.OptionParser(usage="usage: %prog [options]")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address from which messages are received (default %default)")
parser.add_option("-m", "--messages", type="int", default=100,
                  help="number of messages to receive; 0 receives indefinitely (default %default)")
opts, args = parser.parse_args()

try:
    Container(Recv(opts.address, opts.messages)).run()
except KeyboardInterrupt:
    pass
