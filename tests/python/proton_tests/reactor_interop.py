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

from common import Test, free_tcp_port
from proton import Message
from proton.reactor import Reactor
from proton.handlers import CHandshaker, CFlowController

import subprocess
import os
from threading import Thread

class JavaSendThread(Thread):
  def __init__(self, port, count):
    Thread.__init__(self)
    self.port = str(port)
    self.count = str(count)
    self.result = 1

  def run(self):
    self.result = subprocess.call(['java',
        'org.apache.qpid.proton.ProtonJInterop',
        self.port, self.count])

class ReceiveHandler:
  def __init__(self, count):
    self.count = count
    self.handlers = [CHandshaker(), CFlowController()]
    self.messages = []

  def on_reactor_init(self, event):
    port = free_tcp_port()
    self.acceptor = event.reactor.acceptor("localhost", port)
    self.java_thread = JavaSendThread(port, self.count)
    self.java_thread.start()

  def on_delivery(self, event):
    rcv = event.receiver
    msg = Message()
    if rcv and msg.recv(rcv):
      event.delivery.settle()
      self.messages += [msg.body]
      self.count -= 1
      if (self.count == 0):
        self.acceptor.close()

class ReactorInteropTest(Test):

  def setup(self):
    classpath = ""
    if ('CLASSPATH' in os.environ):
      classpath = os.environ['CLASSPATH']
    entries = classpath.split(os.sep)
    self.proton_j_available = len(entries) > 0
    for entry in entries:
      self.proton_j_available |= os.path.exists(entry)

  def protonj_to_protonc(self, count):
    if (not self.proton_j_available):
      raise Skip()

    rh = ReceiveHandler(count)
    r = Reactor(rh)
    r.run()

    rh.java_thread.join()
    assert(rh.java_thread.result == 0)

    for i in range(1, count):
      assert(rh.messages[i-1] == ("message-" + str(i)))

  def test_protonj_to_protonc_1(self):
    self.protonj_to_protonc(1)

  def test_protonj_to_protonc_5(self):
    self.protonj_to_protonc(5)

  def test_protonj_to_protonc_500(self):
    self.protonj_to_protonc(500)

  def test_protonj_to_protonc_5000(self):
    self.protonj_to_protonc(5000)
