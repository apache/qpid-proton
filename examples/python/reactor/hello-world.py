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
from proton.reactor import Reactor

# The proton reactor provides a general purpose event processing
# library for writing reactive programs. A reactive program is defined
# by a set of event handlers. An event handler is just any class or
# object that defines the "on_<event>" methods that it cares to
# handle.

class Program:

    # The reactor init event is produced by the reactor itself when it
    # starts.
    def on_reactor_init(self, event):
        print("Hello, World!")

# When you construct a reactor, you give it a handler.
r = Reactor(Program())

# When you call run, the reactor will process events. The reactor init
# event is what kicks off everything else. When the reactor has no
# more events to process, it exits.
r.run()
