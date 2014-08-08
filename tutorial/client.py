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
from proton_utils import Container, IncomingMessageHandler

class Client(IncomingMessageHandler):
    def __init__(self, container, host, address, requests):
        self.container = container
        self.conn = container.connect(host)
        self.sender = self.conn.sender(address)
        self.receiver = self.conn.receiver(None, dynamic=True, handler=self)
        self.requests = requests

    def next_request(self):
        req = Message(reply_to=self.receiver.remote_source.address, body=self.requests[0])
        self.sender.send_msg(req)

    def on_link_remote_open(self, event):
        self.next_request()

    def on_message(self, event):
        print "%s => %s" % (self.requests.pop(0), event.message.body)
        if self.requests:
            self.next_request()
        else:
            self.conn.close()

    def run(self):
        self.container.run()

REQUESTS= ["Twas brillig, and the slithy toves",
           "Did gire and gymble in the wabe.",
           "All mimsy were the borogroves,",
           "And the mome raths outgrabe."]

Client(Container.DEFAULT, "localhost:5672", "examples", REQUESTS).run()

