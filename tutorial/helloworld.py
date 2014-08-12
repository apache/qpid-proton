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
from proton_events import EventLoop, IncomingMessageHandler

class HelloWorldReceiver(IncomingMessageHandler):
    def on_message(self, event):
        print event.message.body
        event.connection.close()

class HelloWorldSender(object):
    def on_link_flow(self, event):
        event.link.send_msg(Message(body=u"Hello World!"))
        event.link.close()

class HelloWorld(object):
    def __init__(self, eventloop, url, address):
        self.eventloop = eventloop
        self.conn = eventloop.connect(url, handler=self)
        self.address = address

    def on_connection_remote_open(self, event):
        self.conn.receiver(self.address, handler=HelloWorldReceiver())
        self.conn.sender(self.address, handler=HelloWorldSender())

    def on_link_remote_close(self, event):
        self.closed(event.link.remote_condition)

    def on_connection_remote_close(self, event):
        self.closed(event.connection.remote_condition)

    def closed(self, error=None):
        if error:
            print "Closed due to %s" % error
        self.conn.close()

    def run(self):
        self.eventloop.run()

HelloWorld(EventLoop(), "localhost:5672", "examples").run()

