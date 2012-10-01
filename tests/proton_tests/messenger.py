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
    self.running = True
    self.thread.start()

    self.client = Messenger("client")
    self.client.timeout=1000
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

class MessengerTest(Test):

  def run(self):
    msg = Message()
    try:
      while self.running:
        self.server.recv(10)
        while self.server.incoming:
          self.server.get(msg)
          if msg.reply_to:
            msg.address = msg.reply_to
            self.server.put(msg)
    except Timeout:
      print "server timed out"
    self.server.stop()
    self.running = False

  def testSendReceive(self):
    msg = Message()
    msg.address="amqp://0.0.0.0:12345"
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

  def testSendBogus(self):
    msg = Message()
    msg.address="totally-bogus-address"
    try:
      self.client.put(msg)
    except MessengerException, exc:
      err = str(exc)
      assert "unable to send to address: totally-bogus-address (" in err, err
