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

from proctest import *

class LimitedBroker(object):
    def __init__(self, test, fdlimit):
        self.test = test
        self.fdlimit = fdlimit

    def __enter__(self):
        with TestPort() as tp:
            self.port = str(tp.port)
            self.proc = self.test.proc(['prlimit', '-n%d' % self.fdlimit, 'broker', '', self.port])
            self.proc.wait_re("listening")
            return self

    def __exit__(self, *args):
        b = getattr(self, "proc")
        if b:
            if b.poll() not in [1, None]: # Broker crashed or got expected connection error
                raise ProcError(b, "broker crash")
            b.kill()

# Check if we can run prlimit to control resources
try:
    Proc(["prlimit"]).wait_exit()
    has_prlimit = True
except:
    has_prlimit = False

class FdLimitTest(ProcTestCase):

    def setUp(self):
        global has_prlimit
        if not has_prlimit:
            self.skipTest("prlimit not available")
        super(FdLimitTest, self).setUp()

    def proc(self, *args, **kwargs):
        """Skip valgrind for all processes started by this test"""
        return super(FdLimitTest, self).proc(*args, skip_valgrind=True, **kwargs)

    def test_fd_limit_broker(self):
        """Check behaviour when running out of file descriptors on accept"""
        # Not too many FDs but not too few either, some are used for system purposes.
        fdlimit = 256
        with LimitedBroker(self, fdlimit) as b:
            receivers = []
            # Start enough receivers to use all FDs, make sure the broker logs an error
            for i in xrange(fdlimit+1):
                receivers.append(self.proc(["receive", "", b.port, str(i)]))

            # Note: libuv silently swallows EMFILE/ENFILE errors so there is no error reporting.
            # The epoll proactor will close the users connection with the EMFILE/ENFILE error
            if "TRANSPORT_CLOSED" in b.proc.out:
                self.assertIn("open files", b.proc.out)

            # All FDs are now in use, send attempt should fail or hang
            self.assertIn(self.proc(["send", "", b.port, "x"]).poll(), [1, None])

            # Kill receivers to free up FDs
            for r in receivers:
                r.kill()
            for r in receivers:
                r.wait_exit(expect=None)
            # send/receive should succeed now
            self.assertIn("10 messages sent", self.proc(["send", "", b.port]).wait_exit())
            self.assertIn("10 messages received", self.proc(["receive", "", b.port]).wait_exit())

if __name__ == "__main__":
    main()
