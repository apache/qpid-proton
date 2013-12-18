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
    self.server_finite_credit = False
    self.server = Messenger("server")
    self.server.timeout = self.timeout
    self.server.start()
    self.server.subscribe("amqp://~0.0.0.0:12345")
    self.server_thread = Thread(name="server-thread", target=self.run_server)
    self.server_thread.daemon = True
    self.server_is_running_event = Event()
    self.running = True
    self.server_thread_started = False

    self.client = Messenger("client")
    self.client.timeout = self.timeout

  def start(self):
    self.server_thread_started = True
    self.server_thread.start()
    self.server_is_running_event.wait(self.timeout)
    self.client.start()

  def _safelyStopClient(self):
    self.server.interrupt()
    self.client.stop()
    self.client = None

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
    if self.server_finite_credit:
      self._run_server_finite_credit()
    else:
      self._run_server_recv()

  def _run_server_recv(self):
    """ Use recv() to replenish credit each time the server waits
    """
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

  def _run_server_finite_credit(self):
    """ Grant credit once, process until credit runs out
    """
    msg = Message()
    self.server_is_running_event.set()
    try:
      self.server.recv(self.server_credit)
      while self.running:
        try:
          # do not grant additional credit (eg. call recv())
          self.process_incoming(msg)
          self.server.work()
        except Interrupt:
          break
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
    msg.reply_to = "~"
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
      self.server.work(0)
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
    msg.reply_to = "~"
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
    self.server.outgoing_window = size
    self.client.incoming_window = size
    self.start()

    msg = Message()
    msg.address = "amqp://0.0.0.0:12345"
    msg.reply_to = "~"
    msg.subject = "Hello World!"

    for i in range(2*size):
      self.client.put(msg)

    trackers = []
    while len(trackers) < 2*size:
      self.client.recv(2*size - len(trackers))
      while self.client.incoming:
        t = self.client.get(msg)
        assert self.client.status(t) is SETTLED, (t, self.client.status(t))
        trackers.append(t)

    for t in trackers[:size]:
      assert self.client.status(t) is None, (t, self.client.status(t))
    for t in trackers[size:]:
      assert self.client.status(t) is SETTLED, (t, self.client.status(t))

    self.client.accept()

    for t in trackers[:size]:
      assert self.client.status(t) is None, (t, self.client.status(t))
    for t in trackers[size:]:
      assert self.client.status(t) is ACCEPTED, (t, self.client.status(t))

  def testIncomingQueueBiggerThanSessionWindow(self):
    self.testIncomingQueueBiggerThanWindow(2048)

  def testBuffered(self):
    self.client.outgoing_window = 1000
    self.client.incoming_window = 1000
    self.start();
    assert self.server_received == 0
    buffering = 0
    count = 100
    for i in range(count):
      msg = Message()
      msg.address="amqp://0.0.0.0:12345"
      msg.subject="Hello World!"
      msg.body = "First the world, then the galaxy!"
      t = self.client.put(msg)
      buffered = self.client.buffered(t)
      # allow transition from False to True, but not back
      if buffered:
          buffering += 1
      else:
        assert not buffering, ("saw %s buffered deliveries before?" % buffering)

    while self.client.outgoing:
        last = self.client.outgoing
        self.client.send()
        print "sent ", last - self.client.outgoing

    assert self.server_received == count

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
    msg.reply_to = "~"
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
    msg.reply_to = "~"
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
    if not common.isSSLPresent():
        domain = "amqp"
    else:
        domain = "amqps"
    self.server.subscribe(domain + "://~0.0.0.0:12346")
    self.start()
    self.client.route("route1", "amqp://0.0.0.0:12345")
    self.client.route("route2", domain + "://0.0.0.0:12346")

    msg = Message()
    msg.address = "route1"
    msg.reply_to = "~"
    msg.body = "test"
    self.client.put(msg)
    self.client.recv(1)

    reply = Message()
    self.client.get(reply)

    msg = Message()
    msg.address = "route2"
    msg.reply_to = "~"
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
    msg.reply_to = "~"

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
    msg.reply_to = "~"

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
    msg.reply_to = "~"

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

  def testCreditBlockingRebalance(self):
    """ The server is given a fixed amount of credit, and runs until that
    credit is exhausted.
    """
    self.server_finite_credit = True
    self.server_credit = 11
    self.start()

    # put one message out on "Link1" - since there are no other links, it
    # should get all the credit (10 after sending)
    msg = Message()
    msg.address="amqp://0.0.0.0:12345/Link1"
    msg.subject="Hello World!"
    body = "First the world, then the galaxy!"
    msg.body = body
    msg.reply_to = "~"
    self.client.put(msg)
    self.client.send()
    self.client.recv(1)
    assert self.client.incoming == 1

    # Now attempt to exhaust credit using a different link
    for i in range(10):
      msg.address="amqp://0.0.0.0:12345/Link2"
      self.client.put(msg)
    self.client.send()

    deadline = time() + self.timeout
    count = 0
    while count < 11 and time() < deadline:
        self.client.recv(-1)
        while self.client.incoming:
            self.client.get(msg)
            count += 1
    assert count == 11, count

    # now attempt to send one more.  There isn't enough credit, so it should
    # not be sent
    self.client.timeout = 1
    msg.address="amqp://0.0.0.0:12345/Link2"
    self.client.put(msg)
    try:
      self.client.send()
      assert False, "expected client to time out in send()"
    except Timeout:
      pass
    assert self.client.outgoing == 1


class NBMessengerTest(common.Test):

  def setup(self):
    self.client = Messenger("client")
    self.server = Messenger("server")
    self.client.blocking = False
    self.server.blocking = False
    self.server.start()
    self.client.start()
    self.address = "amqp://0.0.0.0:12345"
    self.server.subscribe("amqp://~0.0.0.0:12345")

  def pump(self, timeout=0):
    while self.client.work(0) or self.server.work(0): pass
    self.client.work(timeout)
    self.server.work(timeout)
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

  def testRecvBeforeSubscribe(self):
    self.client.recv()
    self.client.subscribe(self.address + "/foo")

    self.pump()

    msg = Message()
    msg.address = "amqp://client/foo"
    msg.body = "Hello World!"
    self.server.put(msg)

    assert self.client.incoming == 0
    self.pump(self.delay)
    assert self.client.incoming == 1

    msg2 = Message()
    self.client.get(msg2)
    assert msg2.address == msg.address
    assert msg2.body == msg.body

  def testCreditAutoBackpressure(self):
    """ Verify that use of automatic credit (pn_messenger_recv(-1)) does not
    fill the incoming queue indefinitely.  If the receiver does not 'get' the
    message, eventually the sender will block.  See PROTON-350 """
    self.server.recv()
    msg = Message()
    msg.address = self.address
    deadline = time() + self.timeout
    while time() < deadline:
        old = self.server.incoming
        for j in xrange(1001):
            self.client.put(msg)
        self.pump()
        if old == self.server.incoming:
            break;
    assert old == self.server.incoming, "Backpressure not active!"

  def testCreditRedistribution(self):
    """ Verify that a fixed amount of credit will redistribute to new
    links.
    """
    self.server.recv( 5 )

    # first link will get all credit
    msg1 = Message()
    msg1.address = self.address + "/msg1"
    self.client.put(msg1)
    self.pump()
    assert self.server.incoming == 1, self.server.incoming
    assert self.server.receiving == 4, self.server.receiving

    # no credit left over for this link
    msg2 = Message()
    msg2.address = self.address + "/msg2"
    self.client.put(msg2)
    self.pump()
    assert self.server.incoming == 1, self.server.incoming
    assert self.server.receiving == 4, self.server.receiving

    # eventually, credit will rebalance and the new link will send
    deadline = time() + self.timeout
    while time() < deadline:
        sleep(.1)
        self.pump()
        if self.server.incoming == 2:
            break;
    assert self.server.incoming == 2, self.server.incoming
    assert self.server.receiving == 3, self.server.receiving

  def testCreditReclaim(self):
    """ Verify that credit is reclaimed when a link with outstanding credit is
    torn down.
    """
    self.server.recv( 9 )

    # first link will get all credit
    msg1 = Message()
    msg1.address = self.address + "/msg1"
    self.client.put(msg1)
    self.pump()
    assert self.server.incoming == 1, self.server.incoming
    assert self.server.receiving == 8, self.server.receiving

    # no credit left over for this link
    msg2 = Message()
    msg2.address = self.address + "/msg2"
    self.client.put(msg2)
    self.pump()
    assert self.server.incoming == 1, self.server.incoming
    assert self.server.receiving == 8, self.server.receiving

    # and none for this new client
    client2 = Messenger("client2")
    client2.blocking = False
    client2.start()
    msg3 = Message()
    msg3.address = self.address + "/msg3"
    client2.put(msg3)
    while client2.work(0):
        self.pump()
    assert self.server.incoming == 1, self.server.incoming
    assert self.server.receiving == 8, self.server.receiving

    # eventually, credit will rebalance and all links will
    # send a message
    deadline = time() + self.timeout
    while time() < deadline:
        sleep(.1)
        self.pump()
        client2.work(0)
        if self.server.incoming == 3:
            break;
    assert self.server.incoming == 3, self.server.incoming
    assert self.server.receiving == 6, self.server.receiving

    # now tear down client two, this should cause its outstanding credit to be
    # made available to the other links
    client2.stop()
    self.pump()

    for i in range(4):
        self.client.put(msg1)
        self.client.put(msg2)

    # should exhaust all credit
    deadline = time() + self.timeout
    while time() < deadline:
        sleep(.1)
        self.pump()
        if self.server.incoming == 9:
            break;
    assert self.server.incoming == 9, self.server.incoming
    assert self.server.receiving == 0, self.server.receiving

  def testCreditReplenish(self):
    """ When extra credit is available it should be granted to the first
    link that can use it.
    """
    # create three links
    msg = Message()
    for i in range(3):
        msg.address = self.address + "/%d" % i
        self.client.put(msg)

    self.server.recv( 50 )  # 50/3 = 16 per link + 2 extra

    self.pump()
    assert self.server.incoming == 3, self.server.incoming
    assert self.server.receiving == 47, self.server.receiving

    # 47/3 = 15 per link, + 2 extra

    # verify one link can send 15 + the two extra (17)
    for i in range(17):
        msg.address = self.address + "/0"
        self.client.put(msg)
    self.pump()
    assert self.server.incoming == 20, self.server.incoming
    assert self.server.receiving == 30, self.server.receiving

    # now verify that the remaining credit (30) will eventually rebalance
    # across all links (10 per link)
    for j in range(10):
        for i in range(3):
            msg.address = self.address + "/%d" % i
            self.client.put(msg)

    deadline = time() + self.timeout
    while time() < deadline:
        sleep(.1)
        self.pump()
        if self.server.incoming == 50:
            break
    assert self.server.incoming == 50, self.server.incoming
    assert self.server.receiving == 0, self.server.receiving
