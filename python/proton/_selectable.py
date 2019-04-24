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


from ._events import Event

class Selectable(object):

    def __init__(self, delegate, reactor):
        self._delegate = delegate
        self.reading = False
        self.writing = False
        self._deadline = 0
        self._terminal = False
        self._terminated = False
        self._collector = None
        self._reactor = reactor

    def release(self):
        if self._delegate:
            self._delegate.close()

    def __getattr__(self, name):
        if self._delegate:
            return getattr(self._delegate, name)
        else:
            return None

    def _get_deadline(self):
        tstamp = self._deadline
        if tstamp:
            return tstamp
        else:
            return None

    def _set_deadline(self, deadline):
        if not deadline:
            self._deadline = 0
        else:
            self._deadline = deadline

    deadline = property(_get_deadline, _set_deadline)

    def collect(self, collector):
        self._collector = collector

    def push_event(self, context, type):
        if self._collector:
            self._collector.put(context, type)

    def update(self):
        if not self._terminated:
            if self._terminal:
                self._terminated = True
                self.push_event(self, Event.SELECTABLE_FINAL)
            else:
                self.push_event(self, Event.SELECTABLE_UPDATED)

    def readable(self):
        self.push_event(self, Event.SELECTABLE_READABLE)

    def writable(self):
        self.push_event(self, Event.SELECTABLE_WRITABLE)

    def expired(self):
        self.push_event(self, Event.SELECTABLE_EXPIRED)

    @property
    def is_terminal(self):
        return self._terminal

    def terminate(self):
        self._terminal = True
