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

import collections, optparse
from proton import Endpoint, generate_uuid
from proton.handlers import MessagingHandler
from proton.reactor import Container

class Queue(object):
    def __init__(self, dynamic=False):
        self.dynamic = dynamic
        self.queue = collections.deque()
        self.consumers = []

    def subscribe(self, consumer):
        self.consumers.append(consumer)

    def unsubscribe(self, consumer):
        if consumer in self.consumers:
            self.consumers.remove(consumer)
        return len(self.consumers) == 0 and (self.dynamic or self.queue.count == 0)

    def publish(self, message):
        self.queue.append(message)
        self.dispatch()

    def dispatch(self, consumer=None):
        if consumer:
            c = [consumer]
        else:
            c = self.consumers
        while self._deliver_to(c): pass

    def _deliver_to(self, consumers):
        try:
            result = False
            for c in consumers:
                if c.credit:
                    c.send(self.queue.popleft())
                    result = True
            return result
        except IndexError: # no more messages
            return False

class Broker(MessagingHandler):
    def __init__(self, url):
        super(Broker, self).__init__()
        self.url = url
        self.queues = {}

    def on_start(self, event):
        self.acceptor = event.container.listen(self.url)

    def _queue(self, address):
        if address not in self.queues:
            self.queues[address] = Queue()
        return self.queues[address]

    def on_link_opening(self, event):
        if event.link.is_sender:
            if event.link.remote_source.dynamic:
                address = str(generate_uuid())
                event.link.source.address = address
                q = Queue(True)
                self.queues[address] = q
                q.subscribe(event.link)
            elif event.link.remote_source.address:
                event.link.source.address = event.link.remote_source.address
                self._queue(event.link.source.address).subscribe(event.link)
        elif event.link.remote_target.address:
            event.link.target.address = event.link.remote_target.address

    def _unsubscribe(self, link):
        if link.source.address in self.queues and self.queues[link.source.address].unsubscribe(link):
            del self.queues[link.source.address]

    def on_link_closing(self, event):
        if event.link.is_sender:
            self._unsubscribe(event.link)

    def on_connection_closing(self, event):
        self.remove_stale_consumers(event.connection)

    def on_disconnected(self, event):
        self.remove_stale_consumers(event.connection)

    def remove_stale_consumers(self, connection):
        l = connection.link_head(Endpoint.REMOTE_ACTIVE)
        while l:
            if l.is_sender:
                self._unsubscribe(l)
            l = l.next(Endpoint.REMOTE_ACTIVE)

    def on_sendable(self, event):
        self._queue(event.link.source.address).dispatch(event.link)

    def on_message(self, event):
        self._queue(event.link.target.address).publish(event.message)

parser = optparse.OptionParser(usage="usage: %prog [options]")
parser.add_option("-a", "--address", default="localhost:5672",
                  help="address router listens on (default %default)")
opts, args = parser.parse_args()

try:
    Container(Broker(opts.address)).run()
except KeyboardInterrupt: pass
