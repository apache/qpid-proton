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
from proton_events import IncomingMessageHandler
from proton_tornado import TornadoLoop
from tornado.ioloop import IOLoop
import tornado.web

class ExampleHandler(tornado.web.RequestHandler, IncomingMessageHandler):
    def initialize(self, loop):
        self.loop = loop

    def get(self):
        self._write_open()
        self._write_form()
        self._write_close()

    @tornado.web.asynchronous
    def post(self):
        self.conn = self.loop.connect("localhost:5672")
        self.sender = self.conn.sender("examples")
        self.conn.receiver(None, dynamic=True, handler=self)

    def on_link_remote_open(self, event):
        req = Message(reply_to=event.link.remote_source.address, body=self.get_body_argument("message"))
        self.sender.send_msg(req)

    def on_message(self, event):
        self.set_header("Content-Type", "text/html")
        self._write_open()
        self._write_form()
        self.write("Response: " + event.message.body)
        self._write_close()
        self.finish()
        self.conn.close()

    def _write_open(self):
        self.write('<html><body>')

    def _write_close(self):
        self.write('</body></html>')

    def _write_form(self):
        self.write('<form action="/client" method="POST">'
                   'Request: <input type="text" name="message">'
                   '<input type="submit" value="Submit">'
                   '</form>')


loop = TornadoLoop()
app = tornado.web.Application([tornado.web.url(r"/client", ExampleHandler, dict(loop=loop))])
app.listen(8888)
try:
    loop.run()
except KeyboardInterrupt:
    loop.stop()
