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

parser = optparse.OptionParser(usage="usage: %prog [options] <addr_1> ... <addr_n>",
                               description="simple message receiver")
parser.add_option("-c", "--certificate", help="path to certificate file")
parser.add_option("-k", "--private-key", help="path to private key file")
parser.add_option("-p", "--password", help="password for private key file")

opts, args = parser.parse_args()

if not args:
  args = ["amqp://~0.0.0.0"]

mng = Messenger()
mng.certificate=opts.certificate
mng.private_key=opts.private_key
mng.password=opts.password
mng.start()

for a in args:
  mng.subscribe(a)

msg = Message()
while True:
  mng.recv()
  while mng.incoming:
    try:
      mng.get(msg)
    except Exception as e:
      print(e)
    else:
      print(msg.address, msg.subject or "(no subject)", msg.properties, msg.body)

mng.stop()
