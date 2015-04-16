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

# So far the reactive hello-world doesn't look too different from a
# regular old non-reactive hello-world. The on_reactor_init method can
# be used roughly as a 'main' method would. A program that only uses
# that one event, however, isn't going to be very reactive. By using
# other events, we can write a fully reactive program.

class Program:

    # As before we handle the reactor init event.
    def on_reactor_init(self, event):
        print("Hello, World!")

    # In addition to an initial event, the reactor also produces an
    # event when it is about to exit. This may not behave much
    # differently than just putting the goodbye print statement inside
    # on_reactor_init, but as we grow our program, this piece of it
    # will always be what happens last, and will always happen
    # regardless of what other paths the main logic of our program
    # might take.
    def on_reactor_final(self, event):
        print("Goodbye, World!")

r = Reactor(Program())
r.run()
