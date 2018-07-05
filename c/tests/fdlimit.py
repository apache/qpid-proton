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

import os, sys
from subprocess import Popen, PIPE

def wait_listening(p):
    return re.search(b"listening on ([0-9]+)$", p.stdout.readline()).group(1)

class LimitedBroker(Popen):
    def __init__(self, fdlimit):
        super(LimitedBroker, self).__init__(["broker", "", "0"], stdout=PIPE, stderr=open(os.devnull))
        self.fdlimit = fdlimit

    def __enter__(self):
        self.port = wait_listening(self)
        return self

    def __exit__(self, *args):
        self.kill()

# Check if we can run prlimit to control resources
try:
    Proc(["prlimit"]).wait_exit()
except:
    print("Skipping test: prlimit not available")
    sys.exit(0)

class FdLimitTest(ProcTestCase):

    def test_fd_limit_broker(self):
        """Check behaviour when running out of file descriptors on accept"""
        # Not too many FDs but not too few either, some are used for system purposes.
        fdlimit = 256
        with LimitedBroker(fdlimit) as b:
            receivers = []
            # Start enough receivers to use all FDs, make sure the broker logs an error
            for i in range(fdlimit+1):
                receivers.append(Popen(["receive", "", b.port, str(i)], stdout=PIPE))

            # All FDs are now in use, send attempt should fail or hang
            self.assertIn(Popen(["send", "", b.port, "x"], stdout=PIPE, stderr=STDOUT).poll(), [1, None])

            # Kill receivers to free up FDs
            for r in receivers:
                r.kill()
            for r in receivers:
                r.wait()
            # send/receive should succeed now
            self.assertIn("10 messages sent", check_output(["send", "", b.port]))
            self.assertIn("10 messages received", check_output(["receive", "", b.port]))

if __name__ == "__main__":
    main()


