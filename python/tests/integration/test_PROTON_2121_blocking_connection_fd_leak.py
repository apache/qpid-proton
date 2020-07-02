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
PROTON-2121 python-qpid-proton 0.28 BlockingConnection leaks connections (does not close file descriptors)
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import socket
import gc
import os
import subprocess
import threading
import warnings

import cproton

import proton.handlers
import proton.utils
import proton.reactor

from test_unittest import unittest


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
            assert not event.link.remote_source.dynamic, "This cannot happen"
            event.link.source.address = event.link.remote_source.address
        elif event.link.remote_target.address:
            event.link.target.address = event.link.remote_target.address


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


class BlockingConnectionFDLeakTests(unittest.TestCase):
    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    @unittest.expectedFailure
    def test_just_start_stop_test_broker(self):
        with no_fd_leaks(self):
            with test_broker() as broker:
                broker.get_acceptor_sockname()  # wait for acceptor to open

            gc.collect()

    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    @unittest.expectedFailure
    def test_connection_close_all(self):
        with no_fd_leaks(self):
            with test_broker() as broker:
                c = proton.utils.BlockingConnection("{0}:{1}".format(*broker.get_acceptor_sockname()))
                c.close()

            gc.collect()

    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    def test_connection_close_all__do_not_check_test_broker(self):
        with test_broker() as broker:
            acceptor_sockname = broker.get_acceptor_sockname()
            with no_fd_leaks(self):
                c = proton.utils.BlockingConnection("{0}:{1}".format(*acceptor_sockname))
                c.close()

                gc.collect()

    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    @unittest.expectedFailure
    def test_connection_sender_close_all(self):
        with no_fd_leaks(self):
            with test_broker() as broker:
                c = proton.utils.BlockingConnection("{0}:{1}".format(*broker.get_acceptor_sockname()))
                s = c.create_sender("anAddress")
                s.close()
                c.close()

            gc.collect()

    @unittest.skipUnless(*PROC_SELF_FD_EXISTS)
    @unittest.expectedFailure
    def test_connection_receiver_close_all(self):
        with no_fd_leaks(self):
            with test_broker() as broker:
                c = proton.utils.BlockingConnection("{0}:{1}".format(*broker.get_acceptor_sockname()))
                s = c.create_receiver("anAddress")
                s.close()
                c.close()

            gc.collect()
