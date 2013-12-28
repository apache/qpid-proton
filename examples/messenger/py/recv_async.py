#!/usr/bin/python
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
import sys, optparse
from async import *

parser = optparse.OptionParser(usage="usage: %prog [options] <addr_1> ... <addr_n>",
                               description="simple message receiver")

opts, args = parser.parse_args()

if not args:
  args = ["amqp://~0.0.0.0"]

class App(CallbackAdapter):

    def on_start(self):
        print "Started"
        for a in args:
            print "Subscribing to:", a
            self.messenger.subscribe(a)
        self.messenger.recv()

    def on_recv(self, msg):
        print "Received:", msg
        if msg.body == "die":
            self.stop()
        if msg.reply_to:
            self.message.clear()
            self.message.address = msg.reply_to
            self.message.body = "Reply for: %s" % msg.body
            print "Replied:", self.message
            self.send(self.message)

    def on_stop(self):
        print "Stopped"

a = App(Messenger())
a.run()
