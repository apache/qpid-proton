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

    # If an event occurs and its handler doesn't have an on_<event>
    # method, the reactor will attempt to call the on_unhandled method
    # if it exists. This can be useful not only for debugging, but for
    # logging and for delegating/inheritance.
    def on_unhandled(self, name, event):
        print(name, event)

r = Reactor(Program())
r.run()
