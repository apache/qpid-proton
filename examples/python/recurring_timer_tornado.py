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
import time
from proton.reactor import Handler
from proton_tornado import TornadoLoop

class Recurring(Handler):
    def __init__(self, period):
        self.period = period

    def on_start(self, event):
        self.container = event.container
        self.container.schedule(time.time() + self.period, subject=self)

    def on_timer(self, event):
        print("Tick...")
        self.container.schedule(time.time() + self.period, subject=self)

try:
    container = TornadoLoop(Recurring(1.0))
    container.run()
except KeyboardInterrupt:
    container.stop()
    print()


