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
# under the License
#

"""
PROTON-1709 [python] ApplicationEvent causing memory growth
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import platform
import threading
import gc

import proton
from proton.handlers import MessagingHandler
from proton.reactor import Container, ApplicationEvent, EventInjector

from test_unittest import unittest


class Program(MessagingHandler):
    def __init__(self, injector):
        self.injector = injector
        self.counter = 0
        self.on_start_ = threading.Event()

    def on_reactor_init(self, event):
        event.reactor.selectable(self.injector)
        self.on_start_.set()

    def on_count_up(self, event):
        self.counter += 1
        gc.collect()

    def on_done(self, event):
        event.subject.stop()


class Proton1709Test(unittest.TestCase):
    @unittest.skipIf(platform.system() == 'Windows', "TODO jdanek: Test is broken on Windows")
    def test_application_event_no_object_leaks(self):
        event_types_count = len(proton.EventType.TYPES)

        injector = EventInjector()
        p = Program(injector)
        c = Container(p)
        t = threading.Thread(target=c.run)
        t.start()

        p.on_start_.wait()

        object_counts = []

        gc.collect()
        object_counts.append(len(gc.get_objects()))

        for i in range(100):
            injector.trigger(ApplicationEvent("count_up"))

        gc.collect()
        object_counts.append(len(gc.get_objects()))

        self.assertEqual(len(proton.EventType.TYPES), event_types_count + 1)

        injector.trigger(ApplicationEvent("done", subject=c))
        self.assertEqual(len(proton.EventType.TYPES), event_types_count + 2)

        t.join()

        gc.collect()
        object_counts.append(len(gc.get_objects()))

        self.assertEqual(p.counter, 100)

        self.assertTrue(object_counts[1] - object_counts[0] <= 220,
                        "Object counts should not be increasing too fast: {0}".format(object_counts))
        self.assertTrue(object_counts[2] - object_counts[0] <= 10,
                        "No objects should be leaking at the end: {0}".format(object_counts))
