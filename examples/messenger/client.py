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

parser = optparse.OptionParser(usage="usage: %prog <addr> <subject>",
                               description="simple message server")

parser.add_option("-r", "--reply_to", default="replies",
                  help="address: //<domain>[/<name>] (default %default)")

opts, args = parser.parse_args()

if len(args) != 2:
  parser.error("incorrect number of arguments")

address, subject = args

mng = pn_messenger(None)
pn_messenger_start(mng)

msg = pn_message()
pn_message_set_address(msg, address)
pn_message_set_subject(msg, subject)
pn_message_set_reply_to(msg, opts.reply_to)

if pn_messenger_put(mng, msg): print pn_messenger_error(mng)
if pn_messenger_send(mng): print pn_messenger_error(mng)

if opts.reply_to[:2] != "//":
  if pn_messenger_recv(mng, 1): print pn_messenger_error(mng)
  elif pn_messenger_get(mng, msg): print pn_messenger_error(mng)
  else: print pn_message_get_address(msg), pn_message_get_subject(msg)

pn_messenger_stop(mng)
pn_messenger_free(mng)
