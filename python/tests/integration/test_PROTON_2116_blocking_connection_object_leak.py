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
PROTON-2116 Memory leak in python client
PROTON-2192 Memory leak in Python client on Windows
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import platform
import gc
import logging
import os
import subprocess
import sys
import threading
import time
import uuid

import proton.handlers
import proton.reactor
import proton.utils

from test_unittest import unittest

logger = logging.getLogger(__name__)


class ReconnectingTestClient:
    def __init__(self, hostport):
        # type: (str) -> None
        self.hostport = hostport

        self.object_counts = []
        self.done = threading.Event()

    def count_objects(self, message):
        # type: (str) -> None
        gc.collect()
        n = len(gc.get_objects())
        if message == "loop":
            self.object_counts.append(n)
        logger.debug("Message %s, Count %d", message, n)

    def run(self):
        ADDR = "testing123"
        HEARTBEAT = 5
        SLEEP = 5

        recv = None
        conn = None
        for _ in range(3):
            subscribed = False
            while not subscribed:
                try:
                    conn = proton.utils.BlockingConnection(self.hostport, ssl_domain=None, heartbeat=HEARTBEAT)
                    recv = conn.create_receiver(ADDR, name=str(uuid.uuid4()), dynamic=False, options=None)
                    subscribed = True
                except Exception as e:
                    logger.info("received exception %s on connect/subscribe, retry", e)
                    time.sleep(0.5)

            self.count_objects("loop")
            logger.debug("connected")
            while subscribed:
                try:
                    recv.receive(SLEEP)
                except proton.Timeout:
                    pass
                except Exception as e:
                    logger.info(e)
                    try:
                        recv.close()
                        recv = None
                    except:
                        self.count_objects("link close() failed")
                        pass
                    try:
                        conn.close()
                        conn = None
                        self.count_objects("conn closed")
                    except:
                        self.count_objects("conn close() failed")
                        pass
                    subscribed = False
        self.done.set()


class Proton2116Test(unittest.TestCase):
    @unittest.skipIf(platform.system() == 'Windows', "PROTON-2192: The issue is not resolved on Windows")
    def test_blocking_connection_object_leak(self):
        """Kills and restarts broker repeatedly, while client is reconnecting.

        The value of `gc.get_objects()` should not keep increasing in the client.

        These are the automated reproduction steps for PROTON-2116"""
        gc.collect()

        thread = None
        client = None

        host_port = ""  # random on first broker startup
        broker_process = None

        while not client or not client.done.is_set():
            try:
                params = []
                if host_port:
                    params = ['-b', host_port]
                cwd = os.path.dirname(__file__)
                broker_process = subprocess.Popen(
                    args=[sys.executable,
                          os.path.join(cwd, 'broker_PROTON_2116_blocking_connection_object_leak.py')] + params,
                    stdout=subprocess.PIPE,
                    universal_newlines=True,
                )
                host_port = broker_process.stdout.readline()

                if not client:
                    client = ReconnectingTestClient(host_port)
                    thread = threading.Thread(target=client.run)
                    thread.start()

                time.sleep(3)
            finally:
                if broker_process:
                    broker_process.kill()
                    broker_process.wait()
                    broker_process.stdout.close()
            time.sleep(0.3)

        thread.join()

        logger.info("client.object_counts:", client.object_counts)

        # drop first value, it is usually different (before counts settle)
        object_counts = client.object_counts[1:]

        diffs = [c - object_counts[0] for c in object_counts]
        self.assertEqual([0] * 2, diffs, "Object counts should not be increasing")


if __name__ == '__main__':
    unittest.main()
