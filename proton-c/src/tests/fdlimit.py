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
from __future__ import print_function

from proctest import *

def wait_listening(proc):
    m = proc.wait_re("listening on ([0-9]+)$")
    return m.group(1), m.group(0)+"\n" # Return (port, line)

class LimitedBroker(object):
    def __init__(self, test, fdlimit):
        self.test = test
        self.fdlimit = fdlimit

    def __enter__(self):
        self.proc = self.test.proc(["broker", "", "0"])
        self.port, _ = wait_listening(self.proc)
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
except:
    print("Skipping test: prlimit not available")
    sys.exit(0)

class FdLimitTest(ProcTestCase):

    def proc(self, *args, **kwargs):
        """Skip valgrind for all processes started by this test"""
        return super(FdLimitTest, self).proc(*args, valgrind=False, **kwargs)

    def test_fd_limit_broker(self):
        """Check behaviour when running out of file descriptors on accept"""
        # Not too many FDs but not too few either, some are used for system purposes.
        fdlimit = 256
        with LimitedBroker(self, fdlimit) as b:
            receivers = []
            # Start enough receivers to use all FDs, make sure the broker logs an error
            for i in range(fdlimit+1):
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


