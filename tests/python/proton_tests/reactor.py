from __future__ import absolute_import
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

from .common import Test
from proton.reactor import Reactor

class Barf(Exception):
    pass

class BarfOnInit:

    def on_reactor_init(self, event):
        raise Barf()

    def on_connection_init(self, event):
        raise Barf()

    def on_session_init(self, event):
        raise Barf()

    def on_link_init(self, event):
        raise Barf()

class BarfOnTask:

    def on_timer_task(self, event):
        raise Barf()

class BarfOnFinal:

    def on_reactor_final(self, event):
        raise Barf()

class ExceptionTest(Test):

    def setUp(self):
        self.reactor = Reactor()

    def test_reactor_final(self):
        self.reactor.global_handler = BarfOnFinal()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_global_set(self):
        self.reactor.global_handler = BarfOnInit()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_global_add(self):
        self.reactor.global_handler.add(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_reactor_set(self):
        self.reactor.handler = BarfOnInit()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_reactor_add(self):
        self.reactor.handler.add(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection(self):
        self.reactor.connection(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection_set(self):
        c = self.reactor.connection()
        c.handler = BarfOnInit()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection_add(self):
        c = self.reactor.connection()
        c.handler = object()
        c.handler.add(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_session_set(self):
        c = self.reactor.connection()
        s = c.session()
        s.handler = BarfOnInit()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_session_add(self):
        c = self.reactor.connection()
        s = c.session()
        s.handler = object()
        s.handler.add(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_link_set(self):
        c = self.reactor.connection()
        s = c.session()
        l = s.sender("xxx")
        l.handler = BarfOnInit()
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_link_add(self):
        c = self.reactor.connection()
        s = c.session()
        l = s.sender("xxx")
        l.handler = object()
        l.handler.add(BarfOnInit())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_schedule(self):
        self.reactor.schedule(0, BarfOnTask())
        try:
            self.reactor.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_schedule_many_nothings(self):
        class Nothing:
            results = []
            def on_timer_task(self, event):
                self.results.append(None)
        num = 12345
        for a in range(num):
            self.reactor.schedule(0, Nothing())
        self.reactor.run()
        assert len(Nothing.results) == num

    def test_schedule_many_nothing_refs(self):
        class Nothing:
            results = []
            def on_timer_task(self, event):
                self.results.append(None)
        num = 12345
        tasks = []
        for a in range(num):
            tasks.append(self.reactor.schedule(0, Nothing()))
        self.reactor.run()
        assert len(Nothing.results) == num

    def test_schedule_many_nothing_refs_cancel_before_run(self):
        class Nothing:
            results = []
            def on_timer_task(self, event):
                self.results.append(None)
        num = 12345
        tasks = []
        for a in range(num):
            tasks.append(self.reactor.schedule(0, Nothing()))
        for task in tasks:
            task.cancel()
        self.reactor.run()
        assert len(Nothing.results) == 0

    def test_schedule_cancel(self):
        barf = self.reactor.schedule(10, BarfOnTask())
        class CancelBarf:
            def __init__(self, barf):
                self.barf = barf
            def on_timer_task(self, event):
                self.barf.cancel()
                pass
        self.reactor.schedule(0, CancelBarf(barf))
        now = self.reactor.mark()
        try:
            self.reactor.run()
            elapsed = self.reactor.mark() - now
            assert elapsed < 10, "expected cancelled task to not delay the reactor by %s" % elapsed
        except Barf:
            assert False, "expected barf to be cancelled"

    def test_schedule_cancel_many(self):
        num = 12345
        barfs = set()
        for a in range(num):
            barf = self.reactor.schedule(10*(a+1), BarfOnTask())
            class CancelBarf:
                def __init__(self, barf):
                    self.barf = barf
                def on_timer_task(self, event):
                    self.barf.cancel()
                    barfs.discard(self.barf)
                    pass
            self.reactor.schedule(0, CancelBarf(barf))
            barfs.add(barf)
        now = self.reactor.mark()
        try:
            self.reactor.run()
            elapsed = self.reactor.mark() - now
            assert elapsed < num, "expected cancelled task to not delay the reactor by %s" % elapsed
            assert not barfs, "expected all barfs to be discarded"
        except Barf:
            assert False, "expected barf to be cancelled"
