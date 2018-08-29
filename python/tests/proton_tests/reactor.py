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

import time

from proton.reactor import Container, ApplicationEvent, EventInjector
from proton.handlers import Handshaker, MessagingHandler
from proton import Handler, Url

from .common import Test, SkipTest, TestServer, free_tcp_port, ensureCanTestExtendedSASL

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
    init = False

    def on_reactor_init(self, event):
        self.init = True

    def on_reactor_final(self, event):
        raise Barf()

class BarfOnFinalDerived(Handshaker):
    init = False

    def on_reactor_init(self, event):
        self.init = True

    def on_reactor_final(self, event):
        raise Barf()

class ExceptionTest(Test):

    def setUp(self):
        self.container = Container()

    def test_reactor_final(self):
        self.container.global_handler = BarfOnFinal()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_global_set(self):
        self.container.global_handler = BarfOnInit()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_global_add(self):
        self.container.global_handler.add(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_reactor_set(self):
        self.container.handler = BarfOnInit()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_reactor_add(self):
        self.container.handler.add(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection(self):
        self.container.connection(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection_set(self):
        c = self.container.connection()
        c.handler = BarfOnInit()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_connection_add(self):
        c = self.container.connection()
        c.handler = object()
        c.handler.add(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_session_set(self):
        c = self.container.connection()
        s = c.session()
        s.handler = BarfOnInit()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_session_add(self):
        c = self.container.connection()
        s = c.session()
        s.handler = object()
        s.handler.add(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_link_set(self):
        c = self.container.connection()
        s = c.session()
        l = s.sender("xxx")
        l.handler = BarfOnInit()
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_link_add(self):
        c = self.container.connection()
        s = c.session()
        l = s.sender("xxx")
        l.handler = object()
        l.handler.add(BarfOnInit())
        try:
            self.container.run()
            assert False, "expected to barf"
        except Barf:
            pass

    def test_schedule(self):
        self.container.schedule(0, BarfOnTask())
        try:
            self.container.run()
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
            self.container.schedule(0, Nothing())
        self.container.run()
        assert len(Nothing.results) == num

    def test_schedule_many_nothing_refs(self):
        class Nothing:
            results = []
            def on_timer_task(self, event):
                self.results.append(None)
        num = 12345
        tasks = []
        for a in range(num):
            tasks.append(self.container.schedule(0, Nothing()))
        self.container.run()
        assert len(Nothing.results) == num

    def test_schedule_many_nothing_refs_cancel_before_run(self):
        class Nothing:
            results = []
            def on_timer_task(self, event):
                self.results.append(None)
        num = 12345
        tasks = []
        for a in range(num):
            tasks.append(self.container.schedule(0, Nothing()))
        for task in tasks:
            task.cancel()
        self.container.run()
        assert len(Nothing.results) == 0

    def test_schedule_cancel(self):
        barf = self.container.schedule(10, BarfOnTask())
        class CancelBarf:
            def __init__(self, barf):
                self.barf = barf
            def on_timer_task(self, event):
                self.barf.cancel()
                pass
        self.container.schedule(0, CancelBarf(barf))
        now = self.container.mark()
        try:
            self.container.run()
            elapsed = self.container.mark() - now
            assert elapsed < 10, "expected cancelled task to not delay the reactor by %s" % elapsed
        except Barf:
            assert False, "expected barf to be cancelled"

    def test_schedule_cancel_many(self):
        num = 12345
        barfs = set()
        for a in range(num):
            barf = self.container.schedule(10 * (a + 1), BarfOnTask())
            class CancelBarf:
                def __init__(self, barf):
                    self.barf = barf
                def on_timer_task(self, event):
                    self.barf.cancel()
                    barfs.discard(self.barf)
                    pass
            self.container.schedule(0, CancelBarf(barf))
            barfs.add(barf)
        now = self.container.mark()
        try:
            self.container.run()
            elapsed = self.container.mark() - now
            assert elapsed < num, "expected cancelled task to not delay the reactor by %s" % elapsed
            assert not barfs, "expected all barfs to be discarded"
        except Barf:
            assert False, "expected barf to be cancelled"


class ApplicationEventTest(Test):
    """Test application defined events and handlers."""

    class MyTestServer(TestServer):
        def __init__(self):
            super(ApplicationEventTest.MyTestServer, self).__init__()

    class MyHandler(Handler):
        def __init__(self, test):
            super(ApplicationEventTest.MyHandler, self).__init__()
            self._test = test

        def on_hello(self, event):
            # verify PROTON-1056
            self._test.hello_rcvd = str(event)

        def on_goodbye(self, event):
            self._test.goodbye_rcvd = str(event)

    def setUp(self):
        import os
        if not hasattr(os, 'pipe'):
          # KAG: seems like Jython doesn't have an os.pipe() method
          raise SkipTest()
        if os.name=="nt":
          # Correct implementation on Windows is complicated
          raise SkipTest("PROTON-1071")
        self.server = ApplicationEventTest.MyTestServer()
        self.server.reactor.handler.add(ApplicationEventTest.MyHandler(self))
        self.event_injector = EventInjector()
        self.hello_event = ApplicationEvent("hello")
        self.goodbye_event = ApplicationEvent("goodbye")
        self.server.reactor.selectable(self.event_injector)
        self.hello_rcvd = None
        self.goodbye_rcvd = None
        self.server.start()

    def tearDown(self):
        self.server.stop()

    def _wait_for(self, predicate, timeout=10.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if predicate():
                break
            time.sleep(0.1)
        assert predicate()

    def test_application_events(self):
        self.event_injector.trigger(self.hello_event)
        self._wait_for(lambda: self.hello_rcvd is not None)
        self.event_injector.trigger(self.goodbye_event)
        self._wait_for(lambda: self.goodbye_rcvd is not None)


class AuthenticationTestHandler(MessagingHandler):
    def __init__(self):
        super(AuthenticationTestHandler, self).__init__()
        port = free_tcp_port()
        self.url = "localhost:%i" % port
        self.verified = False

    def on_start(self, event):
        self.listener = event.container.listen(self.url)

    def on_connection_opened(self, event):
        event.connection.close()

    def on_connection_opening(self, event):
        assert event.connection.transport.user == "user@proton"
        self.verified = True

    def on_connection_closed(self, event):
        event.connection.close()
        self.listener.close()

    def on_connection_error(self, event):
        event.connection.close()
        self.listener.close()

class ContainerTest(Test):
    """Test container subclass of reactor."""

    def test_event_has_container_attribute(self):
        ensureCanTestExtendedSASL()
        class TestHandler(MessagingHandler):
            def __init__(self):
                super(TestHandler, self).__init__()
                port = free_tcp_port()
                self.url = "localhost:%i" % port

            def on_start(self, event):
                self.listener = event.container.listen(self.url)

            def on_connection_closing(self, event):
                event.connection.close()
                self.listener.close()
        test_handler = TestHandler()
        container = Container(test_handler)
        class ConnectionHandler(MessagingHandler):
            def __init__(self):
                super(ConnectionHandler, self).__init__()

            def on_connection_opened(self, event):
                event.connection.close()
                assert event.container == event.reactor
                assert event.container == container
        container.connect(test_handler.url, handler=ConnectionHandler())
        container.run()

    def test_authentication_via_url(self):
        ensureCanTestExtendedSASL()
        test_handler = AuthenticationTestHandler()
        container = Container(test_handler)
        container.connect("%s:password@%s" % ("user%40proton", test_handler.url), reconnect=False)
        container.run()
        assert test_handler.verified

    def test_authentication_via_container_attributes(self):
        ensureCanTestExtendedSASL()
        test_handler = AuthenticationTestHandler()
        container = Container(test_handler)
        container.user = "user@proton"
        container.password = "password"
        container.connect(test_handler.url, reconnect=False)
        container.run()
        assert test_handler.verified

    def test_authentication_via_kwargs(self):
        ensureCanTestExtendedSASL()
        test_handler = AuthenticationTestHandler()
        container = Container(test_handler)
        container.connect(test_handler.url, user="user@proton", password="password", reconnect=False)
        container.run()
        assert test_handler.verified

    class _ServerHandler(MessagingHandler):
        def __init__(self, host):
            super(ContainerTest._ServerHandler, self).__init__()
            self.host = host
            port = free_tcp_port()
            self.port = free_tcp_port()
            self.client_addr = None
            self.peer_hostname = None

        def on_start(self, event):
            self.listener = event.container.listen("%s:%s" % (self.host, self.port))

        def on_connection_opened(self, event):
            self.client_addr = event.reactor.get_connection_address(event.connection)
            self.peer_hostname = event.connection.remote_hostname

        def on_connection_closing(self, event):
            event.connection.close()
            self.listener.close()

    class _ClientHandler(MessagingHandler):
        def __init__(self):
            super(ContainerTest._ClientHandler, self).__init__()
            self.server_addr = None

        def on_connection_opened(self, event):
            self.server_addr = event.reactor.get_connection_address(event.connection)
            event.connection.close()

    def test_numeric_hostname(self):
        ensureCanTestExtendedSASL()
        server_handler = ContainerTest._ServerHandler("127.0.0.1")
        client_handler = ContainerTest._ClientHandler()
        container = Container(server_handler)
        container.connect(url=Url(host="127.0.0.1",
                                  port=server_handler.port),
                          handler=client_handler)
        container.run()
        assert server_handler.client_addr
        assert client_handler.server_addr
        assert server_handler.peer_hostname == "127.0.0.1", server_handler.peer_hostname
        assert client_handler.server_addr.rsplit(':', 1)[1] == str(server_handler.port)

    def test_non_numeric_hostname(self):
        ensureCanTestExtendedSASL()
        server_handler = ContainerTest._ServerHandler("localhost")
        client_handler = ContainerTest._ClientHandler()
        container = Container(server_handler)
        container.connect(url=Url(host="localhost",
                                  port=server_handler.port),
                          handler=client_handler)
        container.run()
        assert server_handler.client_addr
        assert client_handler.server_addr
        assert server_handler.peer_hostname == "localhost", server_handler.peer_hostname
        assert client_handler.server_addr.rsplit(':', 1)[1] == str(server_handler.port)

    def test_virtual_host(self):
        ensureCanTestExtendedSASL()
        server_handler = ContainerTest._ServerHandler("localhost")
        container = Container(server_handler)
        conn = container.connect(url=Url(host="localhost",
                                         port=server_handler.port),
                                 handler=ContainerTest._ClientHandler(),
                                 virtual_host="a.b.c.org")
        container.run()
        assert server_handler.peer_hostname == "a.b.c.org", server_handler.peer_hostname

    def test_no_virtual_host(self):
        # explicitly setting an empty virtual host should prevent the hostname
        # field from being sent in the Open performative when using the
        # Python Container.
        server_handler = ContainerTest._ServerHandler("localhost")
        container = Container(server_handler)
        conn = container.connect(url=Url(host="localhost",
                                         port=server_handler.port),
                                 handler=ContainerTest._ClientHandler(),
                                 virtual_host="")
        container.run()
        assert server_handler.peer_hostname is None, server_handler.peer_hostname
