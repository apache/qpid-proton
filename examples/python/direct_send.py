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

import optparse
from proton import Message
from proton.handlers import MessagingHandler
from proton.reactor import Container

class Send(MessagingHandler):
    def __init__(self, url):
        super(Send, self).__init__()
        self.url = url
        self.sent = 0
        self.confirmed = 0

    def on_start(self, event):
        event.container.listen(self.url)

    def on_sendable(self, event):
        while event.sender.credit:
            msg = Message(id=(self.sent+1), body={'sequence':(self.sent+1)})
            event.sender.send(msg)
            self.sent += 1

    def on_accepted(self, event):
        self.confirmed += 1

    def on_disconnected(self, event):
        self.sent = self.confirmed

try:
    Container(Send("localhost:8888")).run()
except KeyboardInterrupt: pass
