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

import os, common, xproton
from xproton import *
from threading import Thread

class Test(common.Test):

  def setup(self):
    self.server = pn_messenger("server")
    pn_messenger_set_timeout(self.server, 10000)
    pn_messenger_start(self.server)
    pn_messenger_subscribe(self.server, "//~0.0.0.0:12345")
    self.thread = Thread(target=self.run)
    self.running = True
    self.thread.start()

    self.client = pn_messenger("client")
    pn_messenger_set_timeout(self.client, 10000)
    pn_messenger_start(self.client)

  def teardown(self):
    self.running = False
    msg = pn_message()
    pn_message_set_address(msg, "//0.0.0.0:12345")
    pn_messenger_put(self.client, msg)
    pn_messenger_send(self.client)
    pn_messenger_stop(self.client)
    self.thread.join()
    pn_messenger_free(self.client)
    pn_messenger_free(self.server)
    self.client = None
    self.server = None
    pn_message_free(msg)

class MessengerTest(Test):

  def run(self):
    msg = pn_message()
    while self.running:
      pn_messenger_recv(self.server, 10)
      while pn_messenger_incoming(self.server):
        if pn_messenger_get(self.server, msg):
          print pn_messenger_error(self.server)
        else:
          reply_to = pn_message_get_reply_to(msg)
          if reply_to:
            pn_message_set_address(msg, reply_to)
            pn_messenger_put(self.server, msg)
    pn_messenger_stop(self.server)
    pn_message_free(msg)

  def testSendReceive(self):
    msg = pn_message()
    pn_message_set_address(msg, "//0.0.0.0:12345")
    pn_message_set_subject(msg, "Hello World!")
    body = "First the world, then the galaxy!"
    pn_message_load(msg, body)
    pn_messenger_put(self.client, msg)
    pn_messenger_send(self.client)

    reply = pn_message()
    assert not pn_messenger_recv(self.client, 1)
    assert pn_messenger_incoming(self.client) == 1
    assert not pn_messenger_get(self.client, reply)

    assert pn_message_get_subject(reply) == "Hello World!"
    cd, rbod = pn_message_save(reply, 1024)
    assert not cd
    assert rbod == body, (rbod, body)

    pn_message_free(msg)
    pn_message_free(reply)

  def testSendBogus(self):
    msg = pn_message()
    pn_message_set_address(msg, "totally-bogus-address")
    assert pn_messenger_put(self.client, msg) == PN_ERR
    err = pn_messenger_error(self.client)
    assert "unable to send to address: totally-bogus-address (getaddrinfo:" in err, err
    pn_message_free(msg)
