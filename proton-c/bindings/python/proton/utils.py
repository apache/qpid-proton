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
import Queue, socket, time
from proton import ConnectionException, Endpoint, Handler, Message, Timeout, Url
from proton.reactors import AmqpSocket, Container, Events, SelectLoop, send_msg
from proton.handlers import ScopedHandler

class BlockingLink(object):
    def __init__(self, connection, link):
        self.connection = connection
        self.link = link
        self.connection.wait(lambda: not (self.link.state & Endpoint.REMOTE_UNINIT),
                             msg="Opening link %s" % link.name)

    def close(self):
        self.connection.wait(not (self.link.state & Endpoint.REMOTE_ACTIVE),
                             msg="Closing link %s" % link.name)

    # Access to other link attributes.
    def __getattr__(self, name): return getattr(self.link, name)

class BlockingSender(BlockingLink):
    def __init__(self, connection, sender):
        super(BlockingSender, self).__init__(connection, sender)

    def send_msg(self, msg):
        delivery = send_msg(self.link, msg)
        self.connection.wait(lambda: delivery.settled, msg="Sending on sender %s" % self.link.name)

class BlockingReceiver(BlockingLink):
    def __init__(self, connection, receiver, credit=1):
        super(BlockingReceiver, self).__init__(connection, receiver)
        if credit: receiver.flow(credit)

class BlockingConnection(Handler):
    """
    A synchronous style connection wrapper.
    """
    def __init__(self, url, timeout=None, container=None):
        self.timeout = timeout
        self.container = container or Container()
        if isinstance(url, basestring):
            self.url = Url(url)
        else:
            self.url = url
        self.conn = self.container.connect(url=self.url, handler=self)
        self.wait(lambda: not (self.conn.state & Endpoint.REMOTE_UNINIT),
                  msg="Opening connection")

    def create_sender(self, address, handler=None):
        return BlockingSender(self, self.container.create_sender(self.conn, address, handler=handler))

    def create_receiver(self, address, credit=1, dynamic=False, handler=None):
        return BlockingReceiver(
            self, self.container.create_receiver(self.conn, address, dynamic=dynamic, handler=handler), credit=credit)

    def close(self):
        self.conn.close()
        self.wait(lambda: not (self.conn.state & Endpoint.REMOTE_ACTIVE),
                  msg="Closing connection")

    def run(self):
        """ Hand control over to the event loop (e.g. if waiting indefinitely for incoming messages) """
        self.container.run()

    def wait(self, condition, timeout=False, msg=None):
        """Call do_work until condition() is true"""
        if timeout is False:
            timeout = self.timeout
        if timeout is None:
            while not condition():
                self.container.do_work()
        else:
            deadline = time.time() + timeout
            while not condition():
                if not self.container.do_work(deadline - time.time()):
                    txt = "Connection %s timed out" % self.url
                    if msg: txt += ": " + msg
                    raise Timeout(txt)

    def on_link_remote_close(self, event):
        if event.link.state & Endpoint.LOCAL_ACTIVE:
            self.closed(event.link.remote_condition)

    def on_connection_remote_close(self, event):
        if event.connection.state & Endpoint.LOCAL_ACTIVE:
            self.closed(event.connection.remote_condition)

    def on_disconnected(self, event):
        raise ConnectionException("Connection %s disconnected" % self.url);

    def closed(self, error=None):
        txt = "Connection %s closed" % self.url
        if error:
            txt += " due to: %s" % error
        else:
            txt += " by peer"
        raise ConnectionException(txt)
