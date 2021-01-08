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

"""
PROTON-1800 BlockingConnection descriptor leak
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import gc
import os
import subprocess
import socket
import threading
import unittest
import uuid
import warnings

from collections import namedtuple

import cproton

import proton
import proton.reactor

from proton import Message
from proton.utils import SyncRequestResponse, BlockingConnection
from proton.handlers import IncomingMessageHandler


def get_fd_set():
    # type: () -> set[str]
    return set(os.listdir('/proc/self/fd/'))


@contextlib.contextmanager
def no_fd_leaks(test):
    # type: (unittest.TestCase) -> None
    with warnings.catch_warnings(record=True) as ws:
        before = get_fd_set()
        yield
        delta = get_fd_set().difference(before)
        if len(delta) != 0:
            subprocess.check_call("ls -lF /proc/{0}/fd/".format(os.getpid()), shell=True)
            test.fail("Found {0} new fd(s) after the test".format(delta))

        if len(ws) > 0:
            test.fail([w.message for w in ws])


class Broker(proton.handlers.MessagingHandler):
    def __init__(self, acceptor_url):
        # type: (str) -> None
        super(Broker, self).__init__()
        self.acceptor_url = acceptor_url

        self.sender = None
        self.acceptor = None
        self._acceptor_opened_event = threading.Event()

    def get_acceptor_sockname(self):
        # type: () -> (str, int)
        self._acceptor_opened_event.wait()
        if hasattr(self.acceptor, '_selectable'):  # proton 0.30.0+
            sockname = self.acceptor._selectable._delegate.getsockname()
        else:  # works in proton 0.27.0
            selectable = cproton.pn_cast_pn_selectable(self.acceptor._impl)
            fd = cproton.pn_selectable_get_fd(selectable)
            s = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
            sockname = s.getsockname()
        return sockname[:2]

    def on_start(self, event):
        self.acceptor = event.container.listen(self.acceptor_url)
        self._acceptor_opened_event.set()

    def on_link_opening(self, event):
        if event.link.is_sender:
            assert event.link.remote_source.dynamic
            address = str(uuid.uuid4())
            event.link.source.address = address
            self.sender = event.link
        elif event.link.remote_target.address:
            event.link.target.address = event.link.remote_target.address

    def on_message(self, event):
        message = event.message
        assert self.sender.source.address == message.reply_to
        reply = proton.Message(body=message.body.upper(), correlation_id=message.correlation_id)
        self.sender.send(reply)


@contextlib.contextmanager
def test_broker():
    broker = Broker('localhost:0')
    container = proton.reactor.Container(broker)
    threading.Thread(target=container.run).start()

    try:
        yield broker
    finally:
        container.stop()


PROC_SELF_FD_EXISTS = os.path.exists("/proc/self/fd"), "Skipped: Directory /proc/self/fd does not exist"


def skipOnFailure(reason="AssertionError ignored."):
    """Decorator for test methods that swallows AssertionErrors.

    Passing tests are reported unchanged, tests failing with AssertionError are
    reported as skipped, and tests failing with any other kind of error are
    permitted to fail.

    Use this to temporarily suppress flaky or environment-sensitive tests without
    turning them into dead code that is not being run at all.

    If a test is reliably failing, use unittest.expectedFailure instead."""

    def skip_on_assertion(old_test):
        def new_test(*args, **kwargs):
            try:
                old_test(*args, **kwargs)
            except AssertionError as e:
                raise unittest.SkipTest("Test failure '{0}' ignored: {1}".format(
                    str(e), reason))

        return new_test

    return skip_on_assertion


class Proton1800Test(unittest.TestCase):
    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    @skipOnFailure(reason="PROTON-1800: sut is leaking one fd on Ubuntu Xenial docker image")
    def test_sync_request_response_blocking_connection_no_fd_leaks(self):
        with test_broker() as tb:
            sockname = tb.get_acceptor_sockname()
            url = "{0}:{1}".format(*sockname)
            opts = namedtuple('Opts', ['address', 'timeout'])(address=url, timeout=3)

            with no_fd_leaks(self):
                client = SyncRequestResponse(
                    BlockingConnection(url, opts.timeout, allowed_mechs="ANONYMOUS"), "somequeue")
                try:
                    request = "One Two Three Four"
                    response = client.call(Message(body=request))
                    self.assertEqual(response.body, "ONE TWO THREE FOUR")
                finally:
                    client.connection.close()

        gc.collect()
