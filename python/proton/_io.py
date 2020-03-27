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

import errno
import socket
import select
import time

from ._compat import socket_errno

PN_INVALID_SOCKET = -1

class IO(object):

    @staticmethod
    def _setupsocket(s):
        s.setsockopt(socket.SOL_TCP, socket.TCP_NODELAY, True)
        s.setblocking(False)

    @staticmethod
    def close(s):
        s.close()

    @staticmethod
    def listen(host, port):
        s = socket.socket()
        IO._setupsocket(s)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
        s.bind((host, port))
        s.listen(10)
        return s

    @staticmethod
    def accept(s):
        n = s.accept()
        IO._setupsocket(n[0])
        return n

    @staticmethod
    def connect(addr):
        s = socket.socket(addr[0], addr[1], addr[2])
        IO._setupsocket(s)
        try:
            s.connect(addr[4])
        except socket.error as e:
            if socket_errno(e) not in (errno.EINPROGRESS, errno.EWOULDBLOCK, errno.EAGAIN):
                raise
        return s

    @staticmethod
    def select(*args, **kwargs):
        return select.select(*args, **kwargs)

    @staticmethod
    def sleep(t):
        time.sleep(t)
        return

    class Selector(object):

        def __init__(self):
            self._selectables = set()
            self._reading = set()
            self._writing = set()
            self._deadline = None

        def add(self, selectable):
            self._selectables.add(selectable)
            if selectable.reading:
                self._reading.add(selectable)
            if selectable.writing:
                self._writing.add(selectable)
            if selectable.deadline:
                if self._deadline is None:
                    self._deadline = selectable.deadline
                else:
                    self._deadline = min(selectable.deadline, self._deadline)

        def remove(self, selectable):
            self._selectables.discard(selectable)
            self._reading.discard(selectable)
            self._writing.discard(selectable)
            self.update_deadline()

        @property
        def selectables(self):
            return len(self._selectables)

        def update_deadline(self):
            for sel in self._selectables:
                if sel.deadline:
                    if self._deadline is None:
                        self._deadline = sel.deadline
                    else:
                        self._deadline = min(sel.deadline, self._deadline)

        def update(self, selectable):
            self._reading.discard(selectable)
            self._writing.discard(selectable)
            if selectable.reading:
                self._reading.add(selectable)
            if selectable.writing:
                self._writing.add(selectable)
            self.update_deadline()

        def select(self, timeout):

            def select_inner(timeout):
                # This inner select adds the writing fds to the exception fd set
                # because Windows returns connected fds in the exception set not the
                # writable set
                r = self._reading
                w = self._writing

                now = time.time()

                # No timeout or deadline
                if timeout is None and self._deadline is None:
                    return IO.select(r, w, w)

                if timeout is None:
                    t = max(0, self._deadline - now)
                    return IO.select(r, w, w, t)

                if self._deadline is None:
                    return IO.select(r, w, w, timeout)

                t = max(0, min(timeout, self._deadline - now))
                if len(r)==0 and len(w)==0:
                    if t > 0: IO.sleep(t)
                    return ([],[],[])

                return IO.select(r, w, w, t)

            # Need to allow for signals interrupting us on Python 2
            # In this case the signal handler could have messed up our internal state
            # so don't retry just return with no handles.
            try:
                r, w, ex = select_inner(timeout)
            except select.error as e:
                if socket_errno(e) != errno.EINTR:
                    raise
                r, w, ex = ([], [], [])

            # For windows non blocking connect we get exception not writable so add exceptions to writable
            w += ex

            # Calculate timed out selectables
            now = time.time()
            t = [s for s in self._selectables if s.deadline and now > s.deadline]
            self._deadline = None
            self.update_deadline()
            return r, w, t
