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
from xproton import *

parser = optparse.OptionParser(usage="usage: %prog [options] addr_1 ... addr_n",
                               description="simple message receiver")

opts, args = parser.parse_args()

if not args:
  args = ["//~0.0.0.0"]

m = pn_messenger()
pn_messenger_start(m)

for a in args:
  if pn_messenger_subscribe(m, a):
    print pn_messenger_error(m)
    break

msg = pn_message()
while True:
  if pn_messenger_recv(m, 10):
    print pn_messenger_error(m)
    break
  while pn_messenger_incoming(m):
    if pn_messenger_get(m, msg):
      print pn_messenger_error(m)
    else:
      print "%s: %s" % (pn_message_get_address(msg), pn_message_get_subject(msg))

pn_messenger_stop(m)
pn_messenger_free(m)
