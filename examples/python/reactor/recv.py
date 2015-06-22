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

from __future__ import print_function
from proton import Message
from proton.reactor import Reactor
from proton.handlers import CHandshaker, CFlowController

class Program:

    def __init__(self):
        self.handlers = [CHandshaker(), CFlowController()]
        self.message = Message()

    def on_reactor_init(self, event):
        # Create an amqp acceptor.
        event.reactor.acceptor("0.0.0.0", 5672)
        # There is an optional third argument to the Reactor.acceptor
        # call. Using it, we could supply a handler here that would
        # become the handler for all accepted connections. If we omit
        # it, the reactor simply inherets all the connection events.

    def on_delivery(self, event):
        # XXX: we could make rcv.recv(self.message) work here to
        # compliment the similar thing on send
        rcv = event.receiver
        if rcv and self.message.recv(rcv):
            print(self.message)
            event.delivery.settle()

r = Reactor(Program())
r.run()
