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
from __future__ import absolute_import

from .common import Test, free_tcp_port, Skipped
from proton import Message
from proton.handlers import CHandshaker, CFlowController
from proton.reactor import Reactor

import os
import subprocess
from threading import Thread
import time

class JavaThread(Thread):
  def __init__(self, operation, port, count):
    Thread.__init__(self)
    self.operation = operation
    self.port = str(port)
    self.count = str(count)
    self.result = 1

  def run(self):
    self.result = subprocess.call(['java',
        'org.apache.qpid.proton.ProtonJInterop',
        self.operation, self.port, self.count])

class ReceiveHandler:
  def __init__(self, count):
    self.count = count
    self.handlers = [CHandshaker(), CFlowController()]
    self.messages = []

  def on_reactor_init(self, event):
    port = free_tcp_port()
    self.acceptor = event.reactor.acceptor("127.0.0.1", port)
    self.java_thread = JavaThread("send", port, self.count)
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

class SendHandler:
  def __init__(self, host, num_msgs):
    self.host = host
    self.num_msgs = num_msgs
    self.count = 0
    self.handlers = [CHandshaker()]

  def on_connection_init(self, event):
    conn = event.connection
    conn.hostname = self.host
    ssn = conn.session()
    snd = ssn.sender("sender")
    conn.open()
    ssn.open()
    snd.open()

  def on_link_flow(self, event):
    snd = event.sender
    if snd.credit > 0 and self.count < self.num_msgs:
      self.count += 1
      msg = Message("message-" + str(self.count))
      dlv = snd.send(msg)
      dlv.settle()
      if (self.count == self.num_msgs):
        snd.close()
        snd.session.close()
        snd.connection.close()

  def on_reactor_init(self, event):
    event.reactor.connection(self)

class ReactorInteropTest(Test):

  def setUp(self):
    classpath = ""
    if ('CLASSPATH' in os.environ):
      classpath = os.environ['CLASSPATH']
    entries = classpath.split(os.pathsep)
    self.proton_j_available = False
    for entry in entries:
      self.proton_j_available |= entry != "" and os.path.exists(entry)

  def protonc_to_protonj(self, count):
    if (not self.proton_j_available):
      raise Skipped("ProtonJ not found")

    port = free_tcp_port()
    java_thread = JavaThread("recv", port, count)
    java_thread.start()
    # Give the Java thread time to spin up a JVM and start listening
    # XXX: would be better to parse the stdout output for a message
    time.sleep(1)

    sh = SendHandler('127.0.0.1:' + str(port), count)
    r = Reactor(sh)
    r.run()

    java_thread.join()
    assert(java_thread.result == 0)

  def protonj_to_protonc(self, count):
    if (not self.proton_j_available):
      raise Skipped("ProtonJ not found")

    rh = ReceiveHandler(count)
    r = Reactor(rh)
    r.run()

    rh.java_thread.join()
    assert(rh.java_thread.result == 0)

    for i in range(1, count):
      assert(rh.messages[i-1] == ("message-" + str(i)))

  def test_protonc_to_protonj_1(self):
    self.protonc_to_protonj(1)

  def test_protonc_to_protonj_5(self):
    self.protonc_to_protonj(5)

  def test_protonc_to_protonj_500(self):
    self.protonc_to_protonj(500)

  def test_protonc_to_protonj_5000(self):
    self.protonc_to_protonj(5000)

  def test_protonj_to_protonc_1(self):
    self.protonj_to_protonc(1)

  def test_protonj_to_protonc_5(self):
    self.protonj_to_protonc(5)

  def test_protonj_to_protonc_500(self):
    self.protonj_to_protonc(500)

  def test_protonj_to_protonc_5000(self):
    self.protonj_to_protonc(5000)

