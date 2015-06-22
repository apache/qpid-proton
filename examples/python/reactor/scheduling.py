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

class Program:

    def on_reactor_init(self, event):
        self.start = time.time()
        print("Hello, World!")

        # We can schedule a task event for some point in the future.
        # This will cause the reactor to stick around until it has a
        # chance to process the event.

        # The first argument is the delay. The second argument is the
        # handler for the event. We are just using self for now, but
        # we could pass in another object if we wanted.
        task = event.reactor.schedule(1.0, self)

        # We can ignore the task if we want to, but we can also use it
        # to pass stuff to the handler.
        task.something_to_say = "Yay"

    def on_timer_task(self, event):
        task = event.context # xxx: don't have a task property on event yet
        print(task.something_to_say, "my task is complete!")

    def on_reactor_final(self, event):
        print("Goodbye, World! (after %s long seconds)" % (time.time() - self.start))

r = Reactor(Program())
r.run()
