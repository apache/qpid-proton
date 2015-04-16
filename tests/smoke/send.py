#!/usr/bin/env python
from __future__ import print_function
import sys
from proton import *

messenger = Messenger()
messenger.outgoing_window = 10
message = Message()

address = "0.0.0.0"
if len(sys.argv) > 1:
  address = sys.argv[1]

message.address = address
message.properties = {u"binding": u"python",
                      u"version": sys.version}
message.body = u"Hello World!"

messenger.start()
tracker = messenger.put(message)
print("Put: %s" % message)
messenger.send()
print("Status: %s" % messenger.status(tracker))
messenger.stop()
