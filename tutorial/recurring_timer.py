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

import time
from proton_events import EventLoop, EventDispatcher

class Recurring(EventDispatcher):
    def __init__(self, period):
        self.eventloop = EventLoop(self)
        self.period = period
        self.eventloop.schedule(time.time() + self.period, subject=self)

    def on_timer(self, event):
        print "Tick..."
        self.eventloop.schedule(time.time() + self.period, subject=self)

    def run(self):
        self.eventloop.run()

    def stop(self):
        self.eventloop.stop()

try:
    app = Recurring(1.0)
    app.run()
except KeyboardInterrupt:
    app.stop()
    print


