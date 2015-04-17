#!/usr/bin/env python
from __future__ import print_function
import sys
from proton import *

messenger = Messenger()
messenger.incoming_window = 1
message = Message()

address = "~0.0.0.0"
if len(sys.argv) > 1:
  address = sys.argv[1]
messenger.subscribe(address)

messenger.start()

while True:
  messenger.recv()
  messenger.get(message)
  print("Got: %s" % message)
  messenger.accept()

messenger.stop()
