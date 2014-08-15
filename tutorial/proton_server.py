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
from proton_events import EventLoop, FlowController, IncomingMessageHandler

class Server(IncomingMessageHandler):
    def __init__(self, host, address):
        self.eventloop = EventLoop(self, FlowController(10))
        self.conn = self.eventloop.connect(host)
        self.receiver = self.conn.receiver(address)
        self.senders = {}
        self.relay = None

    def on_message(self, event):
        self.on_request(event.message.body, event.message.reply_to)

    def on_connection_remote_open(self, event):
        if "ANONYMOUS-RELAY" in event.connection.remote_offered_capabilities:
            self.relay = self.conn.sender(None)

    def on_connection_remote_close(self, endpoint, error):
        if error: print "Closed due to %s" % error
        self.conn.close()

    def run(self):
        self.eventloop.run()

    def send(self, response, reply_to):
        sender = self.relay
        if not sender:
            sender = self.senders.get(reply_to)
        if not sender:
            sender = self.conn.sender(reply_to)
            self.senders[reply_to] = sender
        msg = Message(body=response)
        if self.relay:
            msg.address = reply_to
        sender.send_msg(msg)

    def on_request(self, request, reply_to):
        pass

