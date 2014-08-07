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
from proton_utils import ConnectionHandler, SenderHandler, Runtime

class Send(ConnectionHandler, SenderHandler):
    def __init__(self, host, address, messages):
        self.conn = Runtime.DEFAULT.connect(host, handler=self)
        self.sender = self.conn.sender(address, handler=self)
        self.sent = 0
        self.confirmed = 0
        self.total = messages
        self.sender.offered(messages)

    def link_flow(self, event):
        for i in range(self.sender.credit):
            if self.sent == self.total:
                self.sender.drained()
                break
            msg = Message(body={'sequence':self.sent})
            self.sender.send_msg(msg, handler=self)
            self.sent += 1

    def accepted(self, sender, delivery):
        """
        Stop the application once all of the messages are sent and acknowledged,
        """
        self.confirmed += 1
        if self.confirmed == self.total:
            self.sender.close()
            self.conn.close()

    def closed(self, endpoint, error):
        if error:
            print "Closed due to %s" % error
        if endpoint == self.sender:
            self.conn.close()

    def run(self):
        Runtime.DEFAULT.run()

HOST  = "localhost:5672"
ADDRESS  = "examples"
COUNT = 1000

Send(HOST, ADDRESS, COUNT).run()

