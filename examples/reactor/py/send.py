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
from proton import Message
from proton.reactors import Reactor
from proton.handlers import CHandshaker

# This is a send in terms of low level AMQP events. There are handlers
# that can streamline this significantly if you don't want to worry
# about all the details.

class Send:

    def __init__(self, host, message):
        self.host = host
        self.message = message
        # The default event dispatcher will automatically check for a
        # handlers property and delegate the event to all children
        # present.
        self.handlers = [CHandshaker()]

    def on_connection_init(self, event):
        conn = event.connection
        conn.hostname = self.host
        ssn = conn.session()
        snd = ssn.sender("sender")
        conn.open()
        ssn.open()
        snd.open()

    def on_link_flow(self, event):
        snd = event.sender
        if snd.credit > 0:
            snd.send(self.message)
            snd.close()
            snd.session.close()
            snd.connection.close()

class Program:

    def on_reactor_init(self, event):
        # You can use the connection method to create AMQP connections.
        event.reactor.connection(Send(sys.argv[1], Message(body=sys.argv[2])))

r = Reactor(Program())
r.run()
