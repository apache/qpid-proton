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

parser = optparse.OptionParser(usage="usage: %prog <addr> <subject>",
                               description="simple message server")

parser.add_option("-r", "--reply_to", default="~/replies",
                  help="address: [amqp://]<domain>[/<name>] (default %default)")

opts, args = parser.parse_args()

if len(args) != 2:
  parser.error("incorrect number of arguments")

address, subject = args

mng = Messenger()
mng.start()

msg = Message()
msg.address = address
msg.subject = subject
msg.reply_to = opts.reply_to

mng.put(msg)
mng.send()

if opts.reply_to[:2] == "~/":
  mng.recv(1)
  try:
    mng.get(msg)
    print(msg.address, msg.subject)
  except Exception as e:
    print(e)

mng.stop()
