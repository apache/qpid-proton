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

from __future__ import print_function
import tornado.ioloop
import tornado.web
from proton import Message
from proton.handlers import MessagingHandler
from proton_tornado import Container

class Client(MessagingHandler):
    def __init__(self, host, address):
        super(Client, self).__init__()
        self.host = host
        self.address = address
        self.sent = []
        self.pending = []
        self.reply_address = None
        self.sender = None
        self.receiver = None

    def on_start(self, event):
        conn = event.container.connect(self.host)
        self.sender = event.container.create_sender(conn, self.address)
        self.receiver = event.container.create_receiver(conn, None, dynamic=True)

    def on_link_opened(self, event):
        if event.receiver == self.receiver:
            self.reply_address = event.link.remote_source.address
            self.do_request()

    def on_sendable(self, event):
        self.do_request()

    def on_message(self, event):
        if self.sent:
            request, handler = self.sent.pop(0)
            print("%s => %s" % (request, event.message.body))
            handler(event.message.body)
            self.do_request()

    def do_request(self):
        if self.pending and self.reply_address and self.sender.credit:
            request, handler = self.pending.pop(0)
            self.sent.append((request, handler))
            req = Message(reply_to=self.reply_address, body=request)
            self.sender.send(req)

    def request(self, body, handler):
        self.pending.append((body, handler))
        self.do_request()
        self.container.touch()

class ExampleHandler(tornado.web.RequestHandler):
    def initialize(self, client):
        self.client = client

    def get(self):
        self._write_open()
        self._write_form()
        self._write_close()

    @tornado.web.asynchronous
    def post(self):
        client.request(self.get_body_argument("message"), lambda x: self.on_response(x))

    def on_response(self, body):
        self.set_header("Content-Type", "text/html")
        self._write_open()
        self._write_form()
        self.write("Response: " + body)
        self._write_close()
        self.finish()

    def _write_open(self):
        self.write('<html><body>')

    def _write_close(self):
        self.write('</body></html>')

    def _write_form(self):
        self.write('<form action="/client" method="POST">'
                   'Request: <input type="text" name="message">'
                   '<input type="submit" value="Submit">'
                   '</form>')


loop = tornado.ioloop.IOLoop.instance()
client = Client("localhost:5672", "examples")
client.container = Container(client, loop=loop)
client.container.initialise()
app = tornado.web.Application([tornado.web.url(r"/client", ExampleHandler, dict(client=client))])
app.listen(8888)
try:
    loop.start()
except KeyboardInterrupt:
    loop.stop()
