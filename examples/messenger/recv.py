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
import sys, optparse
from proton import *

parser = optparse.OptionParser(usage="usage: %prog [options] <addr_1> ... <addr_n>",
                               description="simple message receiver")

opts, args = parser.parse_args()

if not args:
  args = ["//~0.0.0.0"]

mng = Messenger()
mng.start()

for a in args:
  mng.subscribe(a)

msg = Message()
while True:
  mng.recv(10)
  while mng.incoming:
    try:
      mng.get(msg)
    except Exception, e:
      print e
    else:
      try:
        body = msg.save()
      except Exception, e:
        print e
      else:
        print msg.address, msg.subject or "(no subject)", body

mng.stop()
