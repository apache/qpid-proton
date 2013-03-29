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
from threading import Thread, Event
from time import sleep, time

class Test(common.Test):

  def setup(self):
    # Very high timeout expected to only be exceeded by a genuine functional
    # problem, not by CI server slowness.
    self.long_timeout_millis = 100000

    self.server_credit = 10
    self.server_received = 0
    self.server = Messenger("server")
    self.server.timeout = self.long_timeout_millis
    self.server.start()
    self.server.subscribe("amqp://~0.0.0.0:12345")
    self.server_thread = Thread(name="server-thread", target=self.run_server)
    self.server_thread.daemon = True
    self.server_is_running_event = Event()
    self.running = True

    self.client = Messenger("client")
    self.client.timeout= self.long_timeout_millis

  def start(self):
    self.server_thread.start()
    self.server_is_running_event.wait(self.long_timeout_millis/1000)
    self.client.start()

  def teardown(self):
    try:
      if self.running:
        # send a message to cause the server to promptly exit
        self.running = False
        msg = Message()
        msg.address="amqp://0.0.0.0:12345"
        self.client.put(msg)
        self.client.send()
    finally:
      self.client.stop()
      self.server_thread.join()
      self.client = None
      self.server = None

REJECT_ME = "*REJECT-ME*"

class MessengerTest(Test):

  def run_server(self):
    msg = Message()
    try:
      while self.running:
        self.server_is_running_event.set()
        self.server.recv(self.server_credit)
        self.process_incoming(msg)
    finally:
      self.server.stop()
      self.running = False

  def process_incoming(self, msg):
    while self.server.incoming:
      self.server.get(msg)
      self.server_received += 1
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
      assert False, "Expecting MessengerException"
    except MessengerException, exc:
      err = str(exc)
      assert "unable to send to address: totally-bogus-address" in err, err

  def testOutgoingWindow(self):
    self.server.incoming_window = 10
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

    for i in range(5):
      t = trackers[i]
      assert self.client.status(t) is None, (t, self.client.status(t))

    for i in range(5, 10):
      t = trackers[i]
      assert self.client.status(t) is PENDING, (t, self.client.status(t))

    self.client.send()

    for i in range(5):
      t = trackers[i]
      assert self.client.status(t) is None

    for i in range(5, 10):
      t = trackers[i]
      assert self.client.status(t) is ACCEPTED

  def testReject(self, process_incoming=None):
    if process_incoming:
      self.process_incoming = process_incoming
    self.server.incoming_window = 10
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
    self.server.incoming_window = 10
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
      assert self.client.status(t) is ACCEPTED, (t, self.client.status(t))

    self.client.incoming_window = 10
    remaining = 10

    trackers = []
    while remaining:
      self.client.recv(remaining)
      while self.client.incoming:
        t = self.client.get()
        trackers.append(t)
        self.client.accept(t)
        remaining -= 1
    for t in trackers:
      assert self.client.status(t) is ACCEPTED, (t, self.client.status(t))

  def test_proton222(self):
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"
    msg.load("First the world, then the galaxy!")
    assert self.server_received == 0
    self.client.put(msg)
    self.client.send()
    # ensure the server got the message without requiring client to stop first
    deadline = time() + 10
    while self.server_received == 0:
      assert time() < deadline, "Server did not receive message!"
      sleep(.1)
    assert self.server_received == 1

  def testUnlimitedCredit(self):
    """ Bring up two links.  Verify credit is granted to each link by
    transferring a message over each.
    """
    self.server_credit = -1
    self.start()

    msg = Message()
    msg.address="amqp://0.0.0.0:12345/XXX"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
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

    msg = Message()
    msg.address="amqp://0.0.0.0:12345/YYY"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
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

  def _DISABLE_test_proton268(self):
    """ Reproducer for JIRA Proton-268 """
    """ DISABLED: Causes failure on Jenkins, appears to be unrelated to fix """
    self.server_credit = 2048
    self.start()

    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.load( "X" * 1024 )

    for x in range( 100 ):
      self.client.put( msg )
    self.client.send()

    try:
      self.client.stop()
    except Timeout:
      assert False, "Timeout waiting for client stop()"

    # need to restart client, as teardown() uses it to stop server
    self.client.start()

  def testRoute(self):
    self.server.subscribe("amqps://~0.0.0.0:12346")
    self.start()
    self.client.route("route1", "amqp://0.0.0.0:12345")
    self.client.route("route2", "amqps://0.0.0.0:12346")

    msg = Message()
    msg.address = "route1"
    msg.body = "test"
    self.client.put(msg)
    self.client.recv(1)

    reply = Message()
    self.client.get(reply)

    msg = Message()
    msg.address = "route2"
    msg.body = "test"
    self.client.put(msg)
    self.client.recv(1)

    self.client.get(reply)
    assert reply.body == "test"

  def testDefaultRoute(self):
    self.start()
    self.client.route("*", "amqp://0.0.0.0:12345")

    msg = Message()
    msg.address = "asdf"
    msg.body = "test"

    self.client.put(msg)
    self.client.recv(1)

    reply = Message()
    self.client.get(reply)
    assert reply.body == "test"

  def testDefaultRouteSubstitution(self):
    self.start()
    self.client.route("*", "amqp://0.0.0.0:12345/$1")

    msg = Message()
    msg.address = "asdf"
    msg.body = "test"

    self.client.put(msg)
    self.client.recv(1)

    reply = Message()
    self.client.get(reply)
    assert reply.body == "test"

  def testIncomingRoute(self):
    self.start()
    self.client.route("in", "amqp://~0.0.0.0:12346")
    self.client.subscribe("in")

    msg = Message()
    msg.address = "amqp://0.0.0.0:12345"
    msg.reply_to = "amqp://0.0.0.0:12346"
    msg.body = "test"

    self.client.put(msg)
    self.client.recv(1)
    reply = Message()
    self.client.get(reply)
    assert reply.body == "test"
