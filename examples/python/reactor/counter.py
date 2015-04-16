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
import time
from proton.reactor import Reactor

class Counter:

    def __init__(self, limit):
        self.limit = limit
        self.count = 0

    def on_timer_task(self, event):
        self.count += 1
        print(self.count)
        if self.count < self.limit:
            # A recurring task can be acomplished by just scheduling
            # another event.
            event.reactor.schedule(0.25, self)

class Program:

    def on_reactor_init(self, event):
        self.start = time.time()
        print("Hello, World!")

        # Note that unlike the previous scheduling example, we pass in
        # a separate object for the handler. This means that the timer
        # event we just scheduled will not be seen by Program as it is
        # being handled by the Counter instance we create.
        event.reactor.schedule(0.25, Counter(10))

    def on_reactor_final(self, event):
        print("Goodbye, World! (after %s long seconds)" % (time.time() - self.start))

# In hello-world.py we said the reactor exits when there are no more
# events to process. While this is true, it's not actually complete.
# The reactor exits when there are no more events to process and no
# possibility of future events arising. For that reason the reactor
# will keep running until there are no more scheduled events and then
# exit.
r = Reactor(Program())
r.run()
