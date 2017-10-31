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

import unittest, sys, time
from proctest import *

def python_cmd(name):
    dir = os.path.dirname(__file__)
    return [sys.executable, os.path.join(dir, "..", "..", "python", name)]

MESSAGES=10

def receive_expect_messages(n=MESSAGES): return ''.join(['{"sequence"=%s}\n'%i for i in range(1, n+1)])
def receive_expect_total(n=MESSAGES): return "%s messages received\n"%n
def receive_expect(n=MESSAGES): return receive_expect_messages(n)+receive_expect_total(n)

def send_expect(n=MESSAGES): return "%s messages sent and acknowledged\n" % n
def send_abort_expect(n=MESSAGES): return "%s messages started and aborted\n" % n

class Broker(object):
    def __init__(self, test):
        self.test = test

    def __enter__(self):
        with TestPort() as tp:
            self.port = tp.port
            self.host = tp.host
            self.addr = tp.addr
            self.proc = self.test.proc(["broker", "", self.port])
            self.proc.wait_re("listening")
            return self

    def __exit__(self, *args):
        b = getattr(self, "proc")
        if b:
            if b.poll() !=  None: # Broker crashed
                raise ProcError(b, "broker crash")
            b.kill()

class CExampleTest(ProcTestCase):

    def runex(self, name, port, messages=MESSAGES):
        """Run an example with standard arguments, return output"""
        return self.proc([name, "", str(port), "xtest", str(messages)]).wait_exit()

    def test_send_receive(self):
        """Send first then receive"""
        with Broker(self) as b:
            self.assertEqual(send_expect(), self.runex("send", b.port))
            self.assertMultiLineEqual(receive_expect(), self.runex("receive", b.port))

    def test_receive_send(self):
        """Start receiving  first, then send."""
        with Broker(self) as b:
            self.assertEqual(send_expect(), self.runex("send", b.port))
            self.assertMultiLineEqual(receive_expect(), self.runex("receive", b.port))

    def test_send_direct(self):
        """Send to direct server"""
        with TestPort() as tp:
            d = self.proc(["direct", "", tp.port])
            d.wait_re("listening")
            self.assertEqual(send_expect(), self.runex("send", tp.port))
            self.assertMultiLineEqual("listening\n"+receive_expect(), d.wait_exit())

    def test_receive_direct(self):
        """Receive from direct server"""
        with TestPort() as tp:
            d = self.proc(["direct", "", tp.port])
            d.wait_re("listening")
            self.assertMultiLineEqual(receive_expect(), self.runex("receive", tp.port))
            self.assertEqual("listening\n10 messages sent and acknowledged\n", d.wait_exit())

    def test_send_abort_broker(self):
        """Sending aborted messages to a broker"""
        with Broker(self) as b:
            self.assertEqual(send_expect(), self.runex("send", b.port))
            self.assertEqual(send_abort_expect(), self.runex("send-abort", b.port))
            b.proc.wait_re("Message aborted\n"*MESSAGES)
            self.assertEqual(send_expect(), self.runex("send", b.port))
            expect = receive_expect_messages(MESSAGES)+receive_expect_messages(MESSAGES)+receive_expect_total(20)
            self.assertMultiLineEqual(expect, self.runex("receive", b.port, "20"))

    def test_send_abort_direct(self):
        """Send aborted messages to the direct server"""
        with TestPort() as tp:
            d = self.proc(["direct", "", tp.port, "examples", "20"])
            expect = "listening\n"
            d.wait_re(expect)
            self.assertEqual(send_expect(), self.runex("send", tp.port))
            expect += receive_expect_messages()
            d.wait_re(expect)
            self.assertEqual(send_abort_expect(), self.runex("send-abort", tp.port))
            expect += "Message aborted\n"*MESSAGES
            d.wait_re(expect)
            self.assertEqual(send_expect(), self.runex("send", tp.port))
            expect += receive_expect_messages()+receive_expect_total(20)
            self.maxDiff = None
            self.assertMultiLineEqual(expect, d.wait_exit())

    def test_send_ssl_receive(self):
        """Send first then receive"""
        with Broker(self) as b:
            self.assertEqual(send_expect(), self.runex("send-ssl", b.port))
            self.assertMultiLineEqual(receive_expect(), self.runex("receive", b.port))

if __name__ == "__main__":
    unittest.main()
