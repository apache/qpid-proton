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

# Events know how to dispatch themselves to handlers. By combining
# this with on_unhandled, you can provide a kind of inheritance
# between handlers using delegation.

class Hello:

    def on_reactor_init(self, event):
        print("Hello, World!")

class Goodbye:

    def on_reactor_final(self, event):
        print("Goodbye, World!")

class Program:

    def __init__(self, *delegates):
        self.delegates = delegates

    def on_unhandled(self, name, event):
        for d in self.delegates:
            event.dispatch(d)

r = Reactor(Program(Hello(), Goodbye()))
r.run()
