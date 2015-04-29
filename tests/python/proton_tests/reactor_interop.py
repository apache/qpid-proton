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

from common import Test
from proton import Message
from proton.reactor import Reactor
from proton.handlers import CHandshaker, CFlowController

import subprocess
import os
from threading import Thread

class JavaSendThread(Thread):
  def __init__(self):
    Thread.__init__(self)

  def run(self):
    subprocess.check_output(['java', 'org.apache.qpid.proton.ProtonJInterop'])


class Receive:
  def __init__(self):
    self.handlers = [CHandshaker(), CFlowController()]
    self.message = Message()

  def on_reactor_init(self, event):
    self.acceptor = event.reactor.acceptor("localhost", 56789)
    JavaSendThread().start()

  def on_delivery(self, event):
    rcv = event.receiver
    if rcv and self.message.recv(rcv):
      event.delivery.settle()
      self.acceptor.close()

class ReactorInteropTest(Test):

  def test_protonj_to_protonc(self):
    rcv = Receive()
    r = Reactor(rcv)
    r.run()
    assert(rcv.message.body == "test1")

