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

import time
from proton.reactors import Reactor

class Counter:

    def __init__(self, limit):
        self.limit = limit
        self.count = 0

    def on_timer_task(self, event):
        self.count += 1
        print self.count
        if self.count < self.limit:
            # A recurring task can be acomplished by just scheduling
            # another event.
            event.reactor.schedule(0.25, self)

class Program:

    def on_reactor_init(self, event):
        self.start = time.time()
        print "Hello, World!"
        event.reactor.schedule(0.25, Counter(10))

    def on_reactor_final(self, event):
        print "Goodbye, World! (after %s long seconds)" % (time.time() - self.start)

r = Reactor(Program())
r.run()
