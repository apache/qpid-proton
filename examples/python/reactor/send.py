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

import sys
from proton import Message, Url
from proton.reactor import Reactor
from proton.handlers import CHandshaker

# This is a send in terms of low level AMQP events. There are handlers
# that can streamline this significantly if you don't want to worry
# about all the details, but it is useful to see how the AMQP engine
# classes interact with handlers and events.

class Send:

    def __init__(self, message, target):
        self.message = message
        self.target = target if target is not None else "examples"
        # Use the handlers property to add some default handshaking
        # behaviour.
        self.handlers = [CHandshaker()]

    def on_connection_init(self, event):
        conn = event.connection

        # Every session or link could have their own handler(s) if we
        # wanted simply by setting the "handler" slot on the
        # given session or link.
        ssn = conn.session()

        # If a link doesn't have an event handler, the events go to
        # its parent session. If the session doesn't have a handler
        # the events go to its parent connection. If the connection
        # doesn't have a handler, the events go to the reactor.
        snd = ssn.sender("sender")
        snd.target.address = self.target
        conn.open()
        ssn.open()
        snd.open()

    def on_transport_error(self, event):
        print event.transport.condition

    def on_link_flow(self, event):
        snd = event.sender
        if snd.credit > 0:
            dlv = snd.send(self.message)
            dlv.settle()
            snd.close()
            snd.session.close()
            snd.connection.close()

class Program:

    def __init__(self, url, content):
        self.url = url
        self.content = content

    def on_reactor_init(self, event):
        # You can use the connection method to create AMQP connections.

        # This connection's handler is the Send object. All the events
        # for this connection will go to the Send object instead of
        # going to the reactor. If you were to omit the Send object,
        # all the events would go to the reactor.
        event.reactor.connection_to_host(self.url.host, self.url.port,
                                         Send(Message(self.content),
                                              self.url.path))

args = sys.argv[1:]
url = Url(args.pop() if args else "localhost:5672/examples")
content = args.pop() if args else "Hello World!"

r = Reactor(Program(url, content))
r.run()
