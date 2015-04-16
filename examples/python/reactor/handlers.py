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


class World:

    def on_reactor_init(self, event):
        print("World!")

class Goodbye:

    def on_reactor_final(self, event):
        print("Goodbye, World!")

class Hello:

    def __init__(self):
        # When an event dispatches itself to a handler, it also checks
        # if that handler has a "handlers" attribute and dispatches
        # the event to any children.
        self.handlers = [World(), Goodbye()]

    # The parent handler always receives the event first.
    def on_reactor_init(self, event):
        print("Hello", end=' ')

r = Reactor(Hello())
r.run()
