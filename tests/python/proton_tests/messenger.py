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

import os, common, sys, traceback
from proton import *
from threading import Thread, Event
from time import sleep, time
from common import Skipped

class Test(common.Test):

  def setup(self):
    self.server_credit = 10
    self.server_received = 0
    self.server = Messenger("server")
    self.server.timeout = int(self.timeout*1000)
    self.server.start()
    self.server.subscribe("amqp://~0.0.0.0:12345")
    self.server_thread = Thread(name="server-thread", target=self.run_server)
    self.server_thread.daemon = True
    self.server_is_running_event = Event()
    self.running = True
    self.server_thread_started = False

    self.client = Messenger("client")
    self.client.timeout = int(self.timeout*1000)

  def start(self):
    self.server_thread_started = True
    self.server_thread.start()
    self.server_is_running_event.wait(self.timeout)
    self.client.start()

  def _safelyStopClient(self):
    existing_exception = None
    self.server.interrupt()
    try:
      self.client.stop()
      self.client = None
    except:
      print "Client failed to stop due to: %s" % sys.exc_info()[1]
      if existing_exception:
        raise existing_exception
      else:
        raise

  def teardown(self):
    try:
      if self.running:
        if not self.server_thread_started: self.start()
        # send a message to cause the server to promptly exit
        self.running = False
        self._safelyStopClient()
    finally:
      self.server_thread.join(self.timeout)
      self.server = None

REJECT_ME = "*REJECT-ME*"

class MessengerTest(Test):

  def run_server(self):
    msg = Message()
    try:
      while self.running:
        self.server_is_running_event.set()
        try:
          self.server.recv(self.server_credit)
          self.process_incoming(msg)
        except Interrupt:
          pass
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

  def testSendReceive(self, size=None):
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
    if size is not None:
      while len(body) < size:
        body = 2*body
      body = body[:size]
    msg.body = body
    self.client.put(msg)
    self.client.send()

    reply = Message()
    self.client.recv(1)
    assert self.client.incoming == 1, self.client.incoming
    self.client.get(reply)

    assert reply.subject == "Hello World!"
    rbod = reply.body
    assert rbod == body, (rbod, body)

  def testSendReceive1K(self):
    self.testSendReceive(1024)

  def testSendReceive2K(self):
    self.testSendReceive(2*1024)

  def testSendReceive4K(self):
    self.testSendReceive(4*1024)

  def testSendReceive10K(self):
    self.testSendReceive(10*1024)

  def testSendReceive100K(self):
    self.testSendReceive(100*1024)

  def testSendReceive1M(self):
    self.testSendReceive(1024*1024)

  # PROTON-285 - prevent continually failing test
  def xtestSendBogus(self):
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

    # reduce outgoing_window to 5 and then try to send 10 messages
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

  def testIncomingQueueBiggerThanWindow(self, size=10):
    if IMPLEMENTATION_LANGUAGE == "Java":
      # Currently fails with proton-j. See https://issues.apache.org/jira/browse/PROTON-315
      raise Skipped

    self.server.outgoing_window = size
    self.client.incoming_window = size
    self.start()

    msg = Message()
    msg.address = "amqp://0.0.0.0:12345"
    msg.subject = "Hello World!"

    for i in range(2*size):
      self.client.put(msg)

    trackers = []
    while len(trackers) < 2*size:
      self.client.recv(2*size - len(trackers))
      while self.client.incoming:
        t = self.client.get(msg)
        assert self.client.status(t) is PENDING, (t, self.client.status(t))
        trackers.append(t)

    for t in trackers[:size]:
      assert self.client.status(t) is None, (t, self.client.status(t))
    for t in trackers[size:]:
      assert self.client.status(t) is PENDING, (t, self.client.status(t))

    self.client.accept()

    for t in trackers[:size]:
      assert self.client.status(t) is None, (t, self.client.status(t))
    for t in trackers[size:]:
      assert self.client.status(t) is ACCEPTED, (t, self.client.status(t))

  def testIncomingQueueBiggerThanSessionWindow(self):
    self.testIncomingQueueBiggerThanWindow(2048)

  def test_proton222(self):
    self.start()
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.subject="Hello World!"
    msg.body = "First the world, then the galaxy!"
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
    msg.body = body
    self.client.put(msg)
    self.client.send()

    reply = Message()
    self.client.recv(1)
    assert self.client.incoming == 1
    self.client.get(reply)

    assert reply.subject == "Hello World!"
    rbod = reply.body
    assert rbod == body, (rbod, body)

    msg = Message()
    msg.address="amqp://0.0.0.0:12345/YYY"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
    msg.body = body
    self.client.put(msg)
    self.client.send()

    reply = Message()
    self.client.recv(1)
    assert self.client.incoming == 1
    self.client.get(reply)

    assert reply.subject == "Hello World!"
    rbod = reply.body
    assert rbod == body, (rbod, body)

  def _DISABLE_test_proton268(self):
    """ Reproducer for JIRA Proton-268 """
    """ DISABLED: Causes failure on Jenkins, appears to be unrelated to fix """
    self.server_credit = 2048
    self.start()

    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
    msg.body = "X" * 1024

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

  def echo_address(self, msg):
    while self.server.incoming:
      self.server.get(msg)
      msg.body = msg.address
      self.dispatch(msg)

  def _testRewrite(self, original, rewritten):
    self.start()
    self.process_incoming = self.echo_address
    self.client.route("*", "amqp://0.0.0.0:12345")

    msg = Message()
    msg.address = original
    msg.body = "test"

    self.client.put(msg)
    assert msg.address == original
    self.client.recv(1)
    assert self.client.incoming == 1

    echo = Message()
    self.client.get(echo)
    assert echo.body == rewritten, (echo.body, rewritten)
    assert msg.address == original

  def testDefaultRewriteH(self):
    self._testRewrite("original", "original")

  def testDefaultRewriteUH(self):
    self._testRewrite("user@original", "original")

  def testDefaultRewriteUPH(self):
    self._testRewrite("user:pass@original", "original")

  def testDefaultRewriteHP(self):
    self._testRewrite("original:123", "original:123")

  def testDefaultRewriteUHP(self):
    self._testRewrite("user@original:123", "original:123")

  def testDefaultRewriteUPHP(self):
    self._testRewrite("user:pass@original:123", "original:123")

  def testDefaultRewriteHN(self):
    self._testRewrite("original/name", "original/name")

  def testDefaultRewriteUHN(self):
    self._testRewrite("user@original/name", "original/name")

  def testDefaultRewriteUPHN(self):
    self._testRewrite("user:pass@original/name", "original/name")

  def testDefaultRewriteHPN(self):
    self._testRewrite("original:123/name", "original:123/name")

  def testDefaultRewriteUHPN(self):
    self._testRewrite("user@original:123/name", "original:123/name")

  def testDefaultRewriteUPHPN(self):
    self._testRewrite("user:pass@original:123/name", "original:123/name")

  def testDefaultRewriteSH(self):
    self._testRewrite("amqp://original", "amqp://original")

  def testDefaultRewriteSUH(self):
    self._testRewrite("amqp://user@original", "amqp://original")

  def testDefaultRewriteSUPH(self):
    self._testRewrite("amqp://user:pass@original", "amqp://original")

  def testDefaultRewriteSHP(self):
    self._testRewrite("amqp://original:123", "amqp://original:123")

  def testDefaultRewriteSUHP(self):
    self._testRewrite("amqp://user@original:123", "amqp://original:123")

  def testDefaultRewriteSUPHP(self):
    self._testRewrite("amqp://user:pass@original:123", "amqp://original:123")

  def testDefaultRewriteSHN(self):
    self._testRewrite("amqp://original/name", "amqp://original/name")

  def testDefaultRewriteSUHN(self):
    self._testRewrite("amqp://user@original/name", "amqp://original/name")

  def testDefaultRewriteSUPHN(self):
    self._testRewrite("amqp://user:pass@original/name", "amqp://original/name")

  def testDefaultRewriteSHPN(self):
    self._testRewrite("amqp://original:123/name", "amqp://original:123/name")

  def testDefaultRewriteSUHPN(self):
    self._testRewrite("amqp://user@original:123/name", "amqp://original:123/name")

  def testDefaultRewriteSUPHPN(self):
    self._testRewrite("amqp://user:pass@original:123/name", "amqp://original:123/name")

  def testRewriteSupress(self):
    self.client.rewrite("*", None)
    self._testRewrite("asdf", None)

  def testRewrite(self):
    self.client.rewrite("a", "b")
    self._testRewrite("a", "b")

  def testRewritePattern(self):
    self.client.rewrite("amqp://%@*", "amqp://$2")
    self._testRewrite("amqp://foo@bar", "amqp://bar")

  def testRewriteToAt(self):
    self.client.rewrite("amqp://%/*", "$2@$1")
    self._testRewrite("amqp://domain/name", "name@domain")

  def testRewriteOverrideDefault(self):
    self.client.rewrite("*", "$1")
    self._testRewrite("amqp://user:pass@host", "amqp://user:pass@host")

class NBMessengerTest(common.Test):

  def setup(self):
    self.client = Messenger()
    self.server = Messenger()
    self.client.blocking = False
    self.server.blocking = False
    self.server.start()
    self.client.start()
    self.address = "amqp://0.0.0.0:12345"
    self.server.subscribe("amqp://~0.0.0.0:12345")

  def pump(self):
    while self.client.work(0) or self.server.work(0): pass

  def teardown(self):
    self.server.stop()
    self.client.stop()
    self.pump()
    assert self.server.stopped
    assert self.client.stopped

  def testSmoke(self, count=1):
    self.server.recv()

    msg = Message()
    msg.address = self.address
    for i in range(count):
      msg.body = "Hello %s" % i
      self.client.put(msg)

    msg2 = Message()
    for i in range(count):
      if self.server.incoming == 0:
        self.pump()
      assert self.server.incoming > 0
      self.server.get(msg2)
      assert msg2.body == "Hello %s" % i, (msg2.body, i)

    assert self.client.outgoing == 0, self.client.outgoing
    assert self.server.incoming == 0, self.client.incoming

  def testSmoke1024(self):
    self.testSmoke(1024)

  def testSmoke4096(self):
    self.testSmoke(4096)

  def testPushback(self):
    self.server.recv()

    msg = Message()
    msg.address = self.address
    for i in xrange(16):
      for i in xrange(1024):
        self.client.put(msg)
      self.pump()
      if self.client.outgoing > 0:
        break

    assert self.client.outgoing > 0
