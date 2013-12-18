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
from proton import *

class CallbackAdapter:

    def __init__(self, messenger):
        self.messenger = messenger
        self.messenger.blocking = False
        self.messenger.outgoing_window = 1024
        self.messenger.incoming_window = 1024
        # for application use
        self.message = Message()
        self._incoming_message = Message()
        self.tracked = {}

    def run(self):
        self.running = True
        self.messenger.start()
        self.on_start()

        while self.running:
            self.messenger.work()
            self._process()

        self.messenger.stop()

        while not self.messenger.stopped:
            self.messenger.work()
            self._process()

        self.on_stop()

    def stop(self):
        self.running = False

    def _process(self):
        self._process_outgoing()
        self._process_incoming()

    def _process_outgoing(self):
        for t, on_status in self.tracked.items():
            status = self.messenger.status(t)
            if status != PENDING:
                on_status(status)
                self.messenger.settle(t)
                del self.tracked[t]

    def _process_incoming(self):
        while self.messenger.incoming:
            t = self.messenger.get(self._incoming_message)
            try:
                self.on_recv(self._incoming_message)
                self.messenger.accept(t)
            except:
                ex = sys.exc_info()[1]
                print "Exception:", ex
                self.messenger.reject(t)

    def send(self, message, on_status=None):
        t = self.messenger.put(message)
        if on_status:
            self.tracked[t] = on_status
