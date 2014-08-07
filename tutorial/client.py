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
from proton_utils import ReceiverHandler, Runtime

class Client(ReceiverHandler):
    def __init__(self, host, address, requests):
        self.conn = Runtime.DEFAULT.connect(host)
        self.sender = self.conn.sender(address)
        self.receiver = self.conn.receiver(None, dynamic=True, handler=self)
        self.requests = requests

    def next_request(self):
        req = Message(reply_to=self.receiver.remote_source.address, body=self.requests[0])
        self.sender.send_msg(req)

    def opened(self, receiver):
        self.next_request()

    def received(self, receiver, handle, msg):
        print "%s => %s" % (self.requests.pop(0), msg.body)
        if self.requests:
            self.next_request()
        else:
            self.conn.close()

    def run(self):
        Runtime.DEFAULT.run()

HOST  = "localhost:5672"
ADDRESS  = "examples"
REQUESTS= ["Twas brillig, and the slithy toves",
           "Did gire and gymble in the wabe.",
           "All mimsy were the borogroves,",
           "And the mome raths outgrabe."]

Client(HOST, ADDRESS, REQUESTS).run()

