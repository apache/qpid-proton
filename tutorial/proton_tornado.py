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

from proton_events import ApplicationEvent, Connector, EventLoop, Events, FlowController, MessagingContext, ScopedDispatcher, Url
import tornado.ioloop

class TornadoLoop(EventLoop):
    def __init__(self, *handlers):
        super(TornadoLoop, self).__init__(*handlers)
        self.loop = tornado.ioloop.IOLoop.current()

    def connect(self, url=None, urls=None, address=None, handler=None, reconnect=None):
        conn = super(TornadoLoop, self).connect(url, urls, address, handler, reconnect)
        self.events.process()
        return conn

    def schedule(self, deadline, connection=None, session=None, link=None, delivery=None, subject=None):
        self.loop.call_at(deadline, self.events.dispatch, ApplicationEvent("timer", connection, session, link, delivery, subject))

    def add(self, conn):
        self.loop.add_handler(conn, self._connection_ready, tornado.ioloop.IOLoop.READ | tornado.ioloop.IOLoop.WRITE)

    def remove(self, conn):
        self.loop.remove_handler(conn)

    def run(self):
        self.loop.start()

    def stop(self):
        self.loop.stop()

    def _get_event_flags(self, conn):
        flags = 0
        if conn.reading():
            flags |= tornado.ioloop.IOLoop.READ
        if conn.writing():
            flags |= tornado.ioloop.IOLoop.WRITE
        return flags

    def _connection_ready(self, conn, events):
        if events & tornado.ioloop.IOLoop.READ:
            conn.readable()
        if events & tornado.ioloop.IOLoop.WRITE:
            conn.writable()
        if events & tornado.ioloop.IOLoop.ERROR or conn.closed():
            conn.close()
            self.loop.remove_handler(conn)
            conn.removed()
        self.events.process()
        self.loop.update_handler(conn, self._get_event_flags(conn))
