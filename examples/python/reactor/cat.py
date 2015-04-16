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
import sys, os
from proton.reactor import Reactor

class Echo:

    def __init__(self, source):
        self.source = source

    def on_selectable_init(self, event):
        sel = event.context # XXX: no selectable property yet

        # We can configure a selectable with any file descriptor we want.
        sel.fileno(self.source.fileno())
        # Ask to be notified when the file is readable.
        sel.reading = True
        event.reactor.update(sel)

    def on_selectable_readable(self, event):
        sel = event.context

        # The on_selectable_readable event tells us that there is data
        # to be read, or the end of stream has been reached.
        data = os.read(sel.fileno(), 1024)
        if data:
            print(data, end=' ')
        else:
            sel.terminate()
            event.reactor.update(sel)

class Program:

    def on_reactor_init(self, event):
        event.reactor.selectable(Echo(open(sys.argv[1])))

r = Reactor(Program())
r.run()
