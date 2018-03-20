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

from __future__ import print_function
from proton.reactor import Container, Handler

class Recurring(Handler):
    def __init__(self, period):
        self.period = period

    def on_reactor_init(self, event):
        self.container = event.reactor
        self.container.schedule(self.period, self)

    def on_timer_task(self, event):
        print("Tick...")
        self.container.schedule(self.period, self)

try:
    container = Container(Recurring(1.0))
    container.run()
except KeyboardInterrupt:
    container.stop()
    print()


