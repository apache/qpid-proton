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
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import subprocess
import time

import test_subprocess
from test_unittest import unittest

# Check if we can run prlimit to control resources
try:
    assert subprocess.check_call(["prlimit"], stdout=open(os.devnull, 'w')) == 0, 'prlimit is present, but broken'
    prlimit_available = True
except OSError:
    prlimit_available = False


class PRLimitedBroker(test_subprocess.Server):
    def __init__(self, fdlimit, *args, **kwargs):
        super(PRLimitedBroker, self).__init__(
            ['prlimit', '-n{0:d}:'.format(fdlimit), "broker", "", "0"],  # `-n 256:` sets only soft limit to 256
            stdout=subprocess.PIPE, universal_newlines=True, *args, **kwargs)
        self.fdlimit = fdlimit


class FdLimitTest(unittest.TestCase):
    devnull = open(os.devnull, 'w')

    @classmethod
    def tearDownClass(cls):
        if cls.devnull:
            cls.devnull.close()

    @unittest.skipUnless(prlimit_available, "prlimit not available")
    def test_fd_limit_broker(self):
        """Check behaviour when running out of file descriptors on accept"""
        # Not too many FDs but not too few either, some are used for system purposes.
        fdlimit = 256
        with PRLimitedBroker(fdlimit, kill_me=True) as b:
            receivers = []

            # Start enough receivers to use all FDs
            # NOTE: broker does not log a file descriptor related error at any point in the test, only
            #  PN_TRANSPORT_CLOSED: amqp:connection:framing-error: connection aborted
            #  PN_TRANSPORT_CLOSED: proton:io: Connection reset by peer - disconnected :5672 (connection aborted)
            for i in range(fdlimit + 1):
                receiver = test_subprocess.Popen(["receive", "", b.port, str(i)], stdout=self.devnull)
                receivers.append(receiver)

            # All FDs are now in use, send attempt will (with present implementation) hang
            with test_subprocess.Popen(["send", "", b.port, "x"],
                                       stdout=self.devnull, stderr=subprocess.STDOUT) as sender:
                time.sleep(1)  # polling for None immediately would always succeed, regardless whether send hangs or not
                self.assertIsNone(sender.poll())

                # Kill receivers to free up FDs
                for r in receivers:
                    r.kill()
                for r in receivers:
                    r.wait()

                # Sender now succeeded and exited
                self.assertEqual(sender.wait(), 0)

            # Additional send/receive should succeed now
            self.assertIn("10 messages sent", test_subprocess.check_output(["send", "", b.port], universal_newlines=True))
            self.assertIn("10 messages received", test_subprocess.check_output(["receive", "", b.port], universal_newlines=True))


if __name__ == "__main__":
    unittest.main()
