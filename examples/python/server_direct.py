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
from proton import generate_uuid, Message
from proton.handlers import MessagingHandler
from proton.reactor import Container

class Server(MessagingHandler):
    def __init__(self, url):
        super(Server, self).__init__()
        self.url = url
        self.senders = {}

    def on_start(self, event):
        print("Listening on", self.url)
        self.container = event.container
        self.acceptor = event.container.listen(self.url)

    def on_link_opening(self, event):
        if event.link.is_sender:
            if event.link.remote_source and event.link.remote_source.dynamic:
                event.link.source.address = str(generate_uuid())
                self.senders[event.link.source.address] = event.link
            elif event.link.remote_target and event.link.remote_target.address:
                event.link.target.address = event.link.remote_target.address
                self.senders[event.link.remote_target.address] = event.link
            elif event.link.remote_source:
                event.link.source.address = event.link.remote_source.address
        elif event.link.remote_target:
            event.link.target.address = event.link.remote_target.address

    def on_message(self, event):
        print("Received", event.message)
        sender = self.senders.get(event.message.reply_to)
        if not sender:
            print("No link for reply")
            return
        sender.send(Message(address=event.message.reply_to, body=event.message.body.upper(),
                            correlation_id=event.message.correlation_id))

try:
    Container(Server("0.0.0.0:8888")).run()
except KeyboardInterrupt: pass



