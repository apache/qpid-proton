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
from proton.reactor import Container, Copy
from proton.handlers import MessagingHandler

class Recv(MessagingHandler):
    def __init__(self):
        super(Recv, self).__init__()

    def on_start(self, event):
        conn = event.container.connect("localhost:5672")
        event.container.create_receiver(conn, "examples", options=Copy())

    def on_message(self, event):
        print(event.message)
        if event.receiver.queued == 0 and event.receiver.drained:
            event.connection.close()

try:
    Container(Recv()).run()
except KeyboardInterrupt: pass



