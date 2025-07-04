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
from proton import Message
from proton.handlers import MessagingHandler
from proton.reactor import Container


class Client(MessagingHandler):
    def __init__(self, url, requests):
        super(Client, self).__init__(auto_accept=False)
        self.url = url
        self.requests = requests
        self.outstanding = {}
        self.msgs = 0

    def on_start(self, event):
        self.sender = event.container.create_sender(self.url)
        self.receiver = event.container.create_receiver(self.sender.connection, None, dynamic=True)

    def next_request(self):
        if self.receiver.remote_source.address:
            request = self.requests.pop(0)
            req = Message(reply_to=self.receiver.remote_source.address, correlation_id=self.msgs, body=request)
            self.outstanding[req.correlation_id] = request
            self.msgs += 1
            self.sender.send(req)

    def on_link_opened(self, event):
        if event.receiver == self.receiver:
            self.next_request()

    def on_message(self, event):
        if self.requests:
            self.next_request()

        message = event.message
        delivery = event.delivery
        correlation_id = message.correlation_id
        request = self.outstanding.pop(correlation_id, None)

        if request:
            print(f"{request} => {message.body}")
            self.accept(delivery)
        else:
            print(f"Unexpected response - unknown correlation_id({correlation_id}): {message.body}")
            self.reject(delivery)
        if not self.outstanding:
            event.connection.close()


REQUESTS = ["Twas brillig, and the slithy toves",
            "Did gire and gymble in the wabe.",
            "All mimsy were the borogroves,",
            "And the mome raths outgrabe."]

parser = optparse.OptionParser(usage="usage: %prog [options]",
                               description="Send requests to the supplied address and print responses.")
parser.add_option("-a", "--address", default="localhost:5672/examples",
                  help="address to which messages are sent (default %default)")
opts, args = parser.parse_args()

Container(Client(opts.address, args or REQUESTS)).run()
