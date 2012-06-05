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

import sys
from xproton import PN_ACCEPTED
from util import Constant

TAIL = Constant("TAIL")

class Entry:

  def __init__(self, queue, id, item):
    self.queue = queue
    self.id = id
    self.tag = str(id)
    self.item = item
    self.next = None
    self.acquired = None

  def tail(self):
    return self.item is TAIL

  def append(self, item):
    assert self.next is None
    self.next = Entry(self.queue, self.queue.identify(), item)
    self.queue.size += 1
    return self.next

  def remove(self):
    self.item = None
    self.queue.size -= 1
    self.queue.compact()

  def acquire(self, owner):
    if not self.acquired:
      self.acquired = owner
    return self.acquired

  def release(self):
    if self.acquired:
      self.acquired = None
      for s in self.queue.sources:
        if self.id < s.next.id:
          s.reset()

  def __del__(self):
    if self.item is not None:
      self.queue.size -= 1

  def __repr__(self):
    return "Entry(id=%s, item=%r, %s)" % (self.id, self.item, self.acquired)

class Queue:

  def __init__(self, threshold=None, ring=None, acquire=True, dequeue=True):
    self.next_id = 0
    self.tail = Entry(self, self.identify(), TAIL)
    self.head = self.tail
    self.size = 0
    self.threshold = threshold
    self.ring = ring
    self.acquire = acquire
    self.dequeue = dequeue
    self.sources = []

  def identify(self):
    id = self.next_id
    self.next_id += 1
    return id

  def capacity(self):
    return self.threshold == None or self.size < self.threshold

  def put(self, item):
    entry = self.tail
    self.tail = self.tail.append(TAIL)
    entry.item = item
    if self.ring is not None and self.size > self.ring:
      self.head = self.head.next
    return entry

  def compact(self):
    while self.head.item is None:
      self.head = self.head.next

  def source(self):
    src = Source(self.head, self.acquire, self.dequeue)
    self.sources.append(src)
    return src

  def target(self):
    return Target(self)

  def entries(self):
    e = self.head
    while e is not None:
      yield e
      e = e.next

  def __repr__(self):
    return repr(list(self.entries()))

class Terminus:

  def __init__(self):
    self._durable = False

  def configure(self, definition):
    # XXX: need real sources and targets
    #self._durable = definition.durable
    return definition

  def durable(self):
    return self._durable

  def orphaned(self):
    if self.durable():
      return False
    else:
      self.close()
      return True

class Source(Terminus):

  def __init__(self, next, acquire, dequeue):
    Terminus.__init__(self)
    self.queue = next.queue
    self.next = next
    self.acquire = acquire
    self.dequeue = dequeue
    self.unacked = {}

  def reset(self):
    self.next = self.queue.head

  def get(self):
    while (self.next.next and
           (self.next.item is None or
            self.next.tag in self.unacked or
            (self.acquire and self.next.acquire(self) != self))):
      self.next = self.next.next

    entry = self.next
    if entry.tail():
      return None, None
    self.next = self.next.next

    if self.acquire:
      assert entry.acquired == self

    self.unacked[entry.tag] = entry

    return entry.tag, entry.item

  def resume(self, unsettled):
    oldest = self.next
    for entry in self.unacked.values():
      if entry.id < oldest.id:
        oldest = entry
      if entry.tag not in unsettled:
        self.settle(entry.tag, None)
#    print "RESUME: %s -> %s" % (self.next.id, oldest.id)
    self.next = oldest

  def resuming(self):
    for tag, entry in self.unacked.items():
      yield tag, None

  def settle(self, tag, state):
    entry = self.unacked.pop(tag)

    if state is None:
      entry.release()
    elif self.dequeue:
      entry.remove()
#      print "DEQUEUED:", tag, outcome

    return state

  def close(self):
    for tag in self.unacked.keys():
      # XXX: default outcome
      self.settle(tag, None)
    self.queue.sources.remove(self)

class Target(Terminus):

  def __init__(self, queue):
    Terminus.__init__(self)
    self.queue = queue
    self.unsettled = {}

  def capacity(self):
    return self.queue.capacity()

  def put(self, tag, message, owner=None):
    if tag == "dump":
      print >> sys.stderr, "-------- DUMP START --------"
      for e in self.queue.entries():
        print >> sys.stderr, e
      print >> sys.stderr, "-------- DUMP END   --------"
    elif tag not in self.unsettled:
      entry = self.queue.put(message)
      entry.acquire(owner)
      self.unsettled[tag] = entry
    return PN_ACCEPTED
#    print "ENQUEUED:", tag, message.fragments

  def resume(self, unsettled):
    for tag, entry in self.resuming():
      if tag not in unsettled:
        self.settle(tag, PN_ACCEPTED)

  def resuming(self):
    for tag, entry in self.unsettled.items():
      yield tag, PN_ACCEPTED

  def settle(self, tag, state):
    if tag == "dump":
      return state
    entry = self.unsettled.pop(tag)
    if state is None:
      entry.remove()
    else:
      entry.release()
    return state

  def close(self):
    # XXX: ???
    pass
