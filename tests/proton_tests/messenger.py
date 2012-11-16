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

import os, common
from proton import *
from threading import Thread

class Test(common.Test):

  def setup(self):
    self.server = Messenger("server")
    self.server.timeout=10000
    self.server.start()
    self.server.subscribe("amqp://~0.0.0.0:12345")
    self.thread = Thread(name="server-thread", target=self.run)
    self.thread.daemon = True
    self.running = True

    self.client = Messenger("client")
    self.client.timeout=1000

  def start(self):
    self.thread.start()
    self.client.start()

  def teardown(self):
    if self.running:
      self.running = False
      msg = Message()
      msg.address="amqp://0.0.0.0:12345"
      self.client.put(msg)
      self.client.send()
    self.client.stop()
    self.thread.join()
    self.client = None
    self.server = None

REJECT_ME = "*REJECT-ME*"

class MessengerTest(Test):

  def run(self):
    msg = Message()
    try:
      while self.running:
        self.server.recv(10)
        self.process_incoming(msg)
    except Timeout:
      print "server timed out"
    self.server.stop()
    self.running = False

  def process_incoming(self, msg):
    while self.server.incoming:
      self.server.get(msg)
      if msg.body == REJECT_ME:
        self.server.reject()
      else:
        self.server.accept()
      self.dispatch(msg)

  def dispatch(self, msg):
    if msg.reply_to:
      msg.address = msg.reply_to
      self.server.put(msg)
      self.server.settle()

  def _testSendReceive(self, size=None):
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
    if size is not None:
      while len(body) < size:
        body = 2*body
      body = body[:size]
    msg.load(body)
    self.client.put(msg)
    self.client.send()

    reply = Message()
    self.client.recv(1)
    assert self.client.incoming == 1
    self.client.get(reply)

    assert reply.subject == "Hello World!"
    rbod = reply.save()
    assert rbod == body, (rbod, body)

  def testSendReceive(self):
    self._testSendReceive()

  def testSendReceive1K(self):
    self._testSendReceive(1024)

  def testSendReceive2K(self):
    self._testSendReceive(2*1024)

  def testSendReceive4K(self):
    self._testSendReceive(4*1024)

  def testSendReceive10K(self):
    self._testSendReceive(10*1024)

  def testSendReceive100K(self):
    self._testSendReceive(100*1024)

  def testSendReceive1M(self):
    self._testSendReceive(1024*1024)

  def testSendBogus(self):
    self.start()
    msg = Message()
    msg.address="totally-bogus-address"
    try:
      self.client.put(msg)
    except MessengerException, exc:
      err = str(exc)
      assert "unable to send to address: totally-bogus-address (" in err, err

  def testOutgoingWindow(self):
    self.server.accept_mode = MANUAL
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"

    trackers = []
    for i in range(10):
      trackers.append(self.client.put(msg))

    self.client.send()

    for t in trackers:
      assert self.client.status(t) is None

    self.client.outgoing_window = 5

    trackers = []
    for i in range(10):
      trackers.append(self.client.put(msg))

    for t in trackers:
      assert self.client.status(t) is PENDING

    self.client.send()

    count = 0
    for t in trackers:
      count += 1
      if count > 5:
        assert self.client.status(t) is ACCEPTED
      else:
        assert self.client.status(t) is None

  def testReject(self, process_incoming=None):
    if process_incoming:
      self.process_incoming = process_incoming
    self.server.accept_mode = MANUAL
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"

    self.client.outgoing_window = 10
    trackers = []
    rejected = []
    for i in range(10):
      if i == 5:
        msg.body = REJECT_ME
      else:
        msg.body = "Yay!"
      trackers.append(self.client.put(msg))
      if msg.body == REJECT_ME:
        rejected.append(trackers[-1])

    self.client.send()

    for t in trackers:
      if t in rejected:
        assert self.client.status(t) is REJECTED, (t, self.client.status(t))
      else:
        assert self.client.status(t) is ACCEPTED, (t, self.client.status(t))

  def testRejectIndividual(self):
    self.testReject(self.reject_individual)

  def reject_individual(self, msg):
    if self.server.incoming < 10:
      return
    while self.server.incoming:
      t = self.server.get(msg)
      if msg.body == REJECT_ME:
        self.server.reject(t)
      self.dispatch(msg)
    self.server.accept()


  def testIncomingWindow(self):
    self.server.accept_mode = MANUAL
    self.server.outgoing_window = 10
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"

    self.client.outgoing_window = 10
    trackers = []
    for i in range(10):
      trackers.append(self.client.put(msg))

    self.client.send()

    for t in trackers:
      assert self.client.status(t) is ACCEPTED

    self.client.incoming_window = 10

    remaining = 10

    trackers = []
    while remaining:
      self.client.recv(remaining)
      while self.client.incoming:
        trackers.append(self.client.get())
        remaining -= 1
    for t in trackers:
      assert self.client.status(t) is ACCEPTED
