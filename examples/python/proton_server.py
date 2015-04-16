from __future__ import print_function
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
from proton.reactor import Container
from proton.handlers import MessagingHandler

class Server(MessagingHandler):
    def __init__(self, host, address):
        super(Server, self).__init__()
        self.container = Container(self)
        self.conn = self.container.connect(host)
        self.receiver = self.container.create_receiver(self.conn, address)
        self.senders = {}
        self.relay = None

    def on_message(self, event):
        self.on_request(event.message.body, event.message.reply_to)

    def on_connection_open(self, event):
        if event.connection.remote_offered_capabilities and "ANONYMOUS-RELAY" in event.connection.remote_offered_capabilities:
            self.relay = self.container.create_sender(self.conn, None)

    def on_connection_close(self, endpoint, error):
        if error: print("Closed due to %s" % error)
        self.conn.close()

    def run(self):
        self.container.run()

    def send(self, response, reply_to):
        sender = self.relay
        if not sender:
            sender = self.senders.get(reply_to)
        if not sender:
            sender = self.container.create_sender(self.conn, reply_to)
            self.senders[reply_to] = sender
        msg = Message(body=response)
        if self.relay:
            msg.address = reply_to
        sender.send_msg(msg)

    def on_request(self, request, reply_to):
        pass

