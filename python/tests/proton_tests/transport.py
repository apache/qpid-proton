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

from proton import Connection, Endpoint, Message, Transport, TransportException

from . import common


class Test(common.Test):
    pass


class ClientTransportTest(Test):

    def setUp(self):
        self.transport = Transport()
        self.peer = Transport()
        self.conn = Connection()
        self.peer.bind(self.conn)

    def tearDown(self):
        self.transport = None
        self.peer = None
        self.conn = None

    def drain(self):
        while True:
            p = self.transport.pending()
            if p < 0:
                return
            elif p > 0:
                data = self.transport.peek(p)
                self.peer.push(data)
                self.transport.pop(len(data))
            else:
                assert False

    def assert_error(self, name):
        assert self.conn.remote_container is None, self.conn.remote_container
        self.drain()
        # verify that we received an open frame
        assert self.conn.remote_container is not None, self.conn.remote_container
        # verify that we received a close frame
        assert self.conn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_CLOSED, self.conn.state
        # verify that a framing error was reported
        assert self.conn.remote_condition.name == name, self.conn.remote_condition

    def testEOS(self):
        self.transport.push(b"")  # should be a noop
        self.transport.close_tail()  # should result in framing error
        self.assert_error(u'amqp:connection:framing-error')

    def testPartial(self):
        self.transport.push(b"AMQ")  # partial header
        self.transport.close_tail()  # should result in framing error
        self.assert_error(u'amqp:connection:framing-error')

    def testGarbage(self, garbage=b"GARBAGE_"):
        self.transport.push(garbage)
        self.assert_error(u'amqp:connection:framing-error')
        assert self.transport.pending() < 0
        self.transport.close_tail()
        assert self.transport.pending() < 0

    def testSmallGarbage(self):
        self.testGarbage(b"XXX")

    def testBigGarbage(self):
        self.testGarbage(b"GARBAGE_XXX")

    def testHeader(self):
        self.transport.push(b"AMQP\x00\x01\x00\x00")
        self.transport.close_tail()
        self.assert_error(u'amqp:connection:framing-error')

    def testHeaderBadDOFF1(self):
        """Verify doff > size error"""
        self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x00\x08\x08\x00\x00\x00")

    def testHeaderBadDOFF2(self):
        """Verify doff < 2 error"""
        self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x00\x08\x01\x00\x00\x00")

    def testHeaderBadSize(self):
        """Verify size > max_frame_size error"""
        self.transport.max_frame_size = 512
        self.testGarbage(b"AMQP\x00\x01\x00\x00\x00\x00\x02\x01\x02\x00\x00\x00")

    def testProtocolNotSupported(self):
        self.transport.push(b"AMQP\x01\x01\x0a\x00")
        p = self.transport.pending()
        assert p >= 8, p
        bytes = self.transport.peek(p)
        assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
        self.transport.pop(p)
        self.drain()
        assert self.transport.closed

    def testPeek(self):
        out = self.transport.peek(1024)
        assert out is not None

    def testBindAfterOpen(self):
        conn = Connection()
        ssn = conn.session()
        conn.open()
        ssn.open()
        conn.container = "test-container"
        conn.hostname = "test-hostname"
        trn = Transport()
        trn.bind(conn)
        out = trn.peek(1024)
        assert b"test-container" in out, repr(out)
        assert b"test-hostname" in out, repr(out)
        self.transport.push(out)

        c = Connection()
        assert c.remote_container is None
        assert c.remote_hostname is None
        assert c.session_head(0) is None
        self.transport.bind(c)
        assert c.remote_container == "test-container"
        assert c.remote_hostname == "test-hostname"
        assert c.session_head(0) is not None

    def testCloseHead(self):
        n = self.transport.pending()
        assert n > 0, n
        try:
            self.transport.close_head()
        except TransportException:
            e = sys.exc_info()[1]
            assert "aborted" in str(e), str(e)
        n = self.transport.pending()
        assert n < 0, n

    def testCloseTail(self):
        n = self.transport.capacity()
        assert n > 0, n
        try:
            self.transport.close_tail()
        except TransportException:
            e = sys.exc_info()[1]
            assert "aborted" in str(e), str(e)
        n = self.transport.capacity()
        assert n < 0, n

    def testUnpairedPop(self):
        conn = Connection()
        self.transport.bind(conn)

        conn.hostname = "hostname"
        conn.open()

        dat1 = self.transport.peek(1024)

        ssn = conn.session()
        ssn.open()

        dat2 = self.transport.peek(1024)

        assert dat2[:len(dat1)] == dat1

        snd = ssn.sender("sender")
        snd.open()

        self.transport.pop(len(dat1))
        self.transport.pop(len(dat2) - len(dat1))
        dat3 = self.transport.peek(1024)
        self.transport.pop(len(dat3))
        assert self.transport.peek(1024) == b""

        self.peer.push(dat1)
        self.peer.push(dat2[len(dat1):])
        self.peer.push(dat3)


class ServerTransportTest(Test):

    def setUp(self):
        self.transport = Transport(Transport.SERVER)
        self.peer = Transport()
        self.conn = Connection()
        self.peer.bind(self.conn)

    def tearDOwn(self):
        self.transport = None
        self.peer = None
        self.conn = None

    def drain(self):
        while True:
            p = self.transport.pending()
            if p < 0:
                return
            elif p > 0:
                bytes = self.transport.peek(p)
                self.peer.push(bytes)
                self.transport.pop(len(bytes))
            else:
                assert False

    def assert_error(self, name):
        assert self.conn.remote_container is None, self.conn.remote_container
        self.drain()
        # verify that we received an open frame
        assert self.conn.remote_container is not None, self.conn.remote_container
        # verify that we received a close frame
        assert self.conn.state == Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_CLOSED, self.conn.state
        # verify that a framing error was reported
        assert self.conn.remote_condition.name == name, self.conn.remote_condition

    # TODO: This may no longer be testing anything
    def testEOS(self):
        self.transport.push(b"")  # should be a noop
        self.transport.close_tail()
        self.transport.pending()
        self.drain()
        assert self.transport.closed

    def testPartial(self):
        self.transport.push(b"AMQ")  # partial header
        self.transport.close_tail()
        p = self.transport.pending()
        assert p >= 8, p
        bytes = self.transport.peek(p)
        assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
        self.transport.pop(p)
        self.drain()
        assert self.transport.closed

    def testGarbage(self, garbage=b"GARBAGE_"):
        self.transport.push(garbage)
        p = self.transport.pending()
        assert p >= 8, p
        bytes = self.transport.peek(p)
        assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
        self.transport.pop(p)
        self.drain()
        assert self.transport.closed

    def testSmallGarbage(self):
        self.testGarbage(b"XXX")

    def testBigGarbage(self):
        self.testGarbage(b"GARBAGE_XXX")

    def testHeader(self):
        self.transport.push(b"AMQP\x00\x01\x00\x00")
        self.transport.close_tail()
        self.assert_error(u'amqp:connection:framing-error')

    def testProtocolNotSupported(self):
        self.transport.push(b"AMQP\x01\x01\x0a\x00")
        p = self.transport.pending()
        assert p >= 8, p
        bytes = self.transport.peek(p)
        assert bytes[:8] == b"AMQP\x00\x01\x00\x00"
        self.transport.pop(p)
        self.drain()
        assert self.transport.closed

    def testPeek(self):
        out = self.transport.peek(1024)
        assert out is not None

    def testBindAfterOpen(self):
        conn = Connection()
        ssn = conn.session()
        conn.open()
        ssn.open()
        conn.container = "test-container"
        conn.hostname = "test-hostname"
        trn = Transport()
        trn.bind(conn)
        out = trn.peek(1024)
        assert b"test-container" in out, repr(out)
        assert b"test-hostname" in out, repr(out)
        self.transport.push(out)

        c = Connection()
        assert c.remote_container is None
        assert c.remote_hostname is None
        assert c.session_head(0) is None
        self.transport.bind(c)
        assert c.remote_container == "test-container"
        assert c.remote_hostname == "test-hostname"
        assert c.session_head(0) is not None

    def testCloseHead(self):
        n = self.transport.pending()
        assert n >= 0, n
        try:
            self.transport.close_head()
        except TransportException:
            e = sys.exc_info()[1]
            assert "aborted" in str(e), str(e)
        n = self.transport.pending()
        assert n < 0, n

    def testCloseTail(self):
        n = self.transport.capacity()
        assert n > 0, n
        try:
            self.transport.close_tail()
        except TransportException:
            e = sys.exc_info()[1]
            assert "aborted" in str(e), str(e)
        n = self.transport.capacity()
        assert n < 0, n

    def testUnpairedPop(self):
        conn = Connection()
        self.transport.bind(conn)

        conn.hostname = "hostname"
        conn.open()

        dat1 = self.transport.peek(1024)

        ssn = conn.session()
        ssn.open()

        dat2 = self.transport.peek(1024)

        assert dat2[:len(dat1)] == dat1

        snd = ssn.sender("sender")
        snd.open()

        self.transport.pop(len(dat1))
        self.transport.pop(len(dat2) - len(dat1))
        dat3 = self.transport.peek(1024)
        self.transport.pop(len(dat3))
        assert self.transport.peek(1024) == b""

        self.peer.push(dat1)
        self.peer.push(dat2[len(dat1):])
        self.peer.push(dat3)

    def testEOSAfterSASL(self):
        self.transport.sasl().allowed_mechs('ANONYMOUS')

        self.peer.sasl().allowed_mechs('ANONYMOUS')

        # this should send over the sasl header plus a sasl-init set up
        # for anonymous
        p = self.peer.pending()
        self.transport.push(self.peer.peek(p))
        self.peer.pop(p)

        # now we send EOS
        self.transport.close_tail()

        # the server may send an error back
        p = self.transport.pending()
        while p > 0:
            self.peer.push(self.transport.peek(p))
            self.transport.pop(p)
            p = self.transport.pending()

        # server closed
        assert self.transport.pending() < 0


class LogTest(Test):

    def testTracer(self):
        t = Transport()
        assert t.tracer is None
        messages = []

        def tracer(transport, message):
            messages.append((transport, message))
        t.tracer = tracer
        assert t.tracer is tracer
        t.log("one")
        t.log("two")
        t.log("three")
        assert messages == [(t, "TRACE: one"), (t, "TRACE: two"), (t, "TRACE: three")], messages


class BufferedDeliveryLimitTest(Test):
    """Tests for pn_transport_set_max_buffered_delivery_bytes()."""

    # Build a valid encoded AMQP message of approximately `size` bytes.
    @staticmethod
    def _make_payload(size):
        msg = Message(body=b'x' * size)
        return msg.encode()

    # Set up two in-memory transports wired together (sender → receiver).
    def setUp(self):
        self.sender_conn = Connection()
        self.receiver_conn = Connection()
        self.sender_t = Transport()
        self.receiver_t = Transport()
        self.receiver_t.bind(self.receiver_conn)
        self.sender_t.bind(self.sender_conn)

    def tearDown(self):
        self.sender_conn = None
        self.receiver_conn = None
        self.sender_t = None
        self.receiver_t = None

    def _pump(self):
        from .common import pump
        pump(self.sender_t, self.receiver_t)

    def _open_link(self):
        """Open connection, session and sender/receiver link; return (sender, receiver)."""
        self.sender_conn.open()
        self.receiver_conn.open()
        ssn_s = self.sender_conn.session()
        ssn_s.open()
        self._pump()
        ssn_r = self.receiver_conn.session_head(
            Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE)
        ssn_r.open()
        snd = ssn_s.sender("test")
        snd.open()
        self._pump()
        rcv = self.receiver_conn.link_head(
            Endpoint.LOCAL_UNINIT | Endpoint.REMOTE_ACTIVE)
        rcv.open()
        rcv.flow(100)
        self._pump()
        return snd, rcv

    def test_default_limit_is_nonzero(self):
        """The default limit should be the 4 MiB constant, not unlimited."""
        assert self.receiver_t.max_buffered_delivery_bytes == 4 * 1024 * 1024, \
            self.receiver_t.max_buffered_delivery_bytes

    def test_get_set_roundtrip(self):
        """Getter reflects the value set by the setter."""
        self.receiver_t.max_buffered_delivery_bytes = 1234567
        assert self.receiver_t.max_buffered_delivery_bytes == 1234567
        self.receiver_t.max_buffered_delivery_bytes = 0
        assert self.receiver_t.max_buffered_delivery_bytes == 0

    def test_limit_triggers_resource_error(self):
        """Sending more bytes than the limit closes the connection with resource-limit-exceeded."""
        # Set a very small limit on the receiver side so a single small message exceeds it
        self.receiver_t.max_buffered_delivery_bytes = 16
        snd, rcv = self._open_link()

        payload = self._make_payload(64)  # 64 bytes > 16-byte limit
        snd.delivery(b"d1")
        snd.stream(payload)
        snd.advance()
        self._pump()

        # The receiver sends a Close frame with the error, so the sender
        # sees it as a remote_condition on the sender connection.
        assert self.sender_conn.remote_condition is not None, \
               "Expected sender to see remote resource-limit-exceeded condition"
        assert self.sender_conn.remote_condition.name == u'amqp:resource-limit-exceeded', \
               self.sender_conn.remote_condition

    def test_limit_zero_means_unlimited(self):
        """Setting limit to 0 disables enforcement; large transfers should succeed."""
        self.receiver_t.max_buffered_delivery_bytes = 0
        snd, rcv = self._open_link()

        payload = self._make_payload(1024)
        snd.delivery(b"d1")
        snd.stream(payload)
        snd.advance()
        self._pump()

        # No error should have occurred
        assert self.receiver_conn.condition is None or \
               not self.receiver_conn.condition.name, \
               "Unexpected error: %s" % self.receiver_conn.condition

    def test_reading_clears_buffer_counter(self):
        """After pn_link_recv() the counter decreases and further sends are allowed."""
        payload = self._make_payload(64)
        # Set limit to fit exactly one payload; the second should be blocked unless we read.
        self.receiver_t.max_buffered_delivery_bytes = len(payload) + 64
        snd, rcv = self._open_link()

        # Send first delivery
        snd.delivery(b"d1")
        snd.stream(payload)
        snd.advance()
        self._pump()

        # Consume it on the receiver side via the current delivery
        dlv_r = rcv.current
        while dlv_r and dlv_r.readable:
            chunk = rcv.recv(dlv_r.pending or 1024)
            if not chunk:
                break
        rcv.advance()
        self._pump()

        # Now send a second delivery — should succeed because buffer was freed
        snd.delivery(b"d2")
        snd.stream(payload)
        snd.advance()
        self._pump()

        assert self.receiver_conn.condition is None or \
               not self.receiver_conn.condition.name, \
               "Unexpected error after read: %s" % self.receiver_conn.condition
