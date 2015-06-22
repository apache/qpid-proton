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

    def setup(self):
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
