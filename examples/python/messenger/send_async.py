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
from __future__ import print_function
import sys, optparse
from async import *

parser = optparse.OptionParser(usage="usage: %prog [options] <msg_1> ... <msg_n>",
                               description="simple message sender")
parser.add_option("-a", "--address", default="amqp://0.0.0.0",
                  help="address: //<domain>[/<name>] (default %default)")
parser.add_option("-r", "--reply_to", help="reply_to: //<domain>[/<name>]")

opts, args = parser.parse_args()
if not args:
  args = ["Hello World!"]

class App(CallbackAdapter):

    def on_start(self):
        print("Started")
        self.message.clear()
        self.message.address = opts.address
        self.message.reply_to = opts.reply_to
        for a in args:
            self.message.body = a
            self.send(self.message, self.on_status)

        if opts.reply_to:
            self.messenger.recv()

    def on_status(self, status):
        print("Status:", status)
        if not opts.reply_to or opts.reply_to[0] != "~":
            args.pop(0)
            if not args: self.stop()

    def on_recv(self, msg):
        print("Received:", msg)
        if opts.reply_to and opts.reply_to[0] == "~":
            args.pop(0)
            if not args: self.stop()

    def on_stop(self):
        print("Stopped")

a = App(Messenger())
a.run()
