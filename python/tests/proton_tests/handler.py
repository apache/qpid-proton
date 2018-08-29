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

from __future__ import absolute_import

import os, gc, traceback

from proton import *
from proton.reactor import Container

from . import common

CUSTOM = EventType("custom")

class HandlerTest(common.Test):
  def test_reactorHandlerCycling(self, n=0):

    class CustomHandler(Handler):
      UNSET = 999999999
      def __init__(self):
        self.offset = len(traceback.extract_stack())
      def on_reactor_init(self, event):
        self.depth = len(traceback.extract_stack())
      def reset(self):
        self.depth = self.UNSET
      @property
      def init_depth(self):
        d = self.depth - self.offset
        return d
    custom = CustomHandler()

    container = Container()
    container.handler = custom
    for i in range(n):
      h = container.handler
      container.handler = h
    custom.reset()
    container.run()
    assert custom.init_depth < 50, "Unexpectedly long traceback for a simple handler"

  def test_reactorHandlerCycling10k(self):
    self.test_reactorHandlerCycling(10000)

  def test_reactorHandlerCycling100(self):
    self.test_reactorHandlerCycling(100)

  def do_customEvent(self, reactor_handler, event_root):

    class CustomHandler:
      did_custom = False
      did_init = False
      def __init__(self, *handlers):
        self.handlers = handlers
      def on_reactor_init(self, event):
        self.did_init = True
      def on_custom(self, event):
        self.did_custom = True

    class CustomInvoker(CustomHandler):
      def on_reactor_init(self, event):
        h = event_root(event)
        event.dispatch(h, CUSTOM)
        self.did_init = True

    child = CustomInvoker()
    root = CustomHandler(child)

    container = Container()

    reactor_handler(container, root)
    container.run()
    assert root.did_init
    assert child.did_init
    assert root.did_custom
    assert child.did_custom

  def set_root(self, reactor, root):
    reactor.handler = root
  def add_root(self, reactor, root):
    reactor.handler.add(root)
  def append_root(self, reactor, root):
    reactor.handler.handlers.append(root)

  def event_root(self, event):
    return event.root

  def event_reactor_handler(self, event):
    return event.reactor.handler

  def test_set_handler(self):
    self.do_customEvent(self.set_root, self.event_reactor_handler)

  def test_add_handler(self):
    self.do_customEvent(self.add_root, self.event_reactor_handler)

  def test_append_handler(self):
    self.do_customEvent(self.append_root, self.event_reactor_handler)

  def test_set_root(self):
    self.do_customEvent(self.set_root, self.event_root)

  def test_add_root(self):
    self.do_customEvent(self.add_root, self.event_root)

  def test_append_root(self):
    self.do_customEvent(self.append_root, self.event_root)
