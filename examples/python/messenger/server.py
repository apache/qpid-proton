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
from __future__ import print_function
import sys, optparse
from proton import *

parser = optparse.OptionParser(usage="usage: %prog <addr_1> ... <addr_n>",
                               description="simple message server")

opts, args = parser.parse_args()

if not args:
  args = ["amqp://~0.0.0.0"]

mng = Messenger()
mng.start()

for a in args:
  mng.subscribe(a)

def dispatch(request, response):
  if request.subject:
    response.subject = "Re: %s" % request.subject
  response.properties = request.properties
  print("Dispatched %s %s" % (request.subject, request.properties))

msg = Message()
reply = Message()

while True:
  if mng.incoming < 10:
    mng.recv(10)

  if mng.incoming > 0:
    mng.get(msg)
    if msg.reply_to:
      print(msg.reply_to)
      reply.address = msg.reply_to
      reply.correlation_id = msg.correlation_id
      reply.body = msg.body
    dispatch(msg, reply)
    mng.put(reply)
    mng.send()

mng.stop()
