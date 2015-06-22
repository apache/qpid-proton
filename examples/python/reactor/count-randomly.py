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
import time, random
from proton.reactor import Reactor

# Let's try to modify our counter example. In addition to counting to
# 10 in quarter second intervals, let's also print out a random number
# every half second. This is not a super easy thing to express in a
# purely sequential program, but not so difficult using events.

class Counter:

    def __init__(self, limit):
        self.limit = limit
        self.count = 0

    def on_timer_task(self, event):
        self.count += 1
        print(self.count)
        if not self.done():
            event.reactor.schedule(0.25, self)

    # add a public API to check for doneness
    def done(self):
        return self.count >= self.limit

class Program:

    def on_reactor_init(self, event):
        self.start = time.time()
        print("Hello, World!")

        # Save the counter instance in an attribute so we can refer to
        # it later.
        self.counter = Counter(10)
        event.reactor.schedule(0.25, self.counter)

        # Now schedule another event with a different handler. Note
        # that the timer tasks go to separate handlers, and they don't
        # interfere with each other.
        event.reactor.schedule(0.5, self)

    def on_timer_task(self, event):
        # keep on shouting until we are done counting
        print("Yay, %s!" % random.randint(10, 100))
        if not self.counter.done():
            event.reactor.schedule(0.5, self)

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
