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

# This is a test script to run the examples and verify that they behave as expected.

from exampletest import *

import unittest
import sys

def python_cmd(name):
    dir = os.path.dirname(__file__)
    return [sys.executable, os.path.join(dir, "..", "..", "python", name)]

def receive_expect(n):
    return ''.join('{"sequence"=%s}\n'%i for i in xrange(1, n+1)) + "%s messages received\n"%n

class CExampleTest(BrokerTestCase):
    broker_exe = ["libuv_broker"]

    def test_send_receive(self):
        """Send first then receive"""
        s = self.proc(["libuv_send", "-a", self.addr])
        self.assertEqual("100 messages sent and acknowledged\n", s.wait_out())
        r = self.proc(["libuv_receive", "-a", self.addr])
        self.assertEqual(receive_expect(100), r.wait_out())

    def test_receive_send(self):
        """Start receiving  first, then send."""
        r = self.proc(["libuv_receive", "-a", self.addr]);
        s = self.proc(["libuv_send", "-a", self.addr]);
        self.assertEqual("100 messages sent and acknowledged\n", s.wait_out())
        self.assertEqual(receive_expect(100), r.wait_out())

if __name__ == "__main__":
    unittest.main()
