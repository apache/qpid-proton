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

from proton import Message
from proton_utils import FlowController, Handshaker, ReceiverHandler, Runtime

HOST  = "localhost:8888"
ADDRESS  = "examples"

class HelloWorld(ReceiverHandler):
    def received(self, receiver, delivery, msg):
        print msg.body
        receiver.connection.close()

runtime = Runtime(HelloWorld(), Handshaker(), FlowController(1))
runtime.listen(HOST)
conn = runtime.connect(HOST)
conn.sender(ADDRESS).send_msg(Message(body=u"Hello World!"))
runtime.run()

