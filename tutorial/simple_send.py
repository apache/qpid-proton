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
import proton_events

class Send(proton_events.BaseHandler):
    def __init__(self, messages):
        self.sent = 0
        self.confirmed = 0
        self.total = messages

    def on_credit(self, event):
        while event.link.credit and self.sent < self.total:
            msg = Message(body={'sequence':(self.sent+1)})
            event.link.send_msg(msg)
            self.sent += 1

    def on_accepted(self, event):
        self.confirmed += 1
        if self.confirmed == self.total:
            print "all messages confirmed"
            event.connection.close()

    def on_disconnected(self, event):
        self.sent = self.confirmed

try:
    conn = proton_events.connect("localhost:5672", handler=Send(10000))
    conn.create_sender("examples")
    proton_events.run()
except KeyboardInterrupt: pass
