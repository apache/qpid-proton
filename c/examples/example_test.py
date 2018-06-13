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

def wait_listening(proc):
    m = proc.wait_re("listening on ([0-9]+)$")
    return m.group(1), m.group(0)+"\n" # Return (port, line)

class Broker(object):
    def __init__(self, test):
        self.test = test

    def __enter__(self):
        self.proc = self.test.proc(["broker", "", "0"])
        self.port, _ = wait_listening(self.proc)
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
        d = self.proc(["direct", "", "0"])
        port, line = wait_listening(d)
        self.assertEqual(send_expect(), self.runex("send", port))
        self.assertMultiLineEqual(line+receive_expect(), d.wait_exit())

    def test_receive_direct(self):
        """Receive from direct server"""
        d = self.proc(["direct", "", "0"])
        port, line = wait_listening(d)
        self.assertMultiLineEqual(receive_expect(), self.runex("receive", port))
        self.assertEqual(line+"10 messages sent and acknowledged\n", d.wait_exit())

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
        d = self.proc(["direct", "", "0", "examples", "20"])
        port, line = wait_listening(d)
        expect = line
        self.assertEqual(send_expect(), self.runex("send", port))
        expect += receive_expect_messages()
        d.wait_re(expect)
        self.assertEqual(send_abort_expect(), self.runex("send-abort", port))
        expect += "Message aborted\n"*MESSAGES
        d.wait_re(expect)
        self.assertEqual(send_expect(), self.runex("send", port))
        expect += receive_expect_messages()+receive_expect_total(20)
        self.maxDiff = None
        self.assertMultiLineEqual(expect, d.wait_exit())

    def test_send_ssl_receive(self):
        """Send with SSL, then receive"""
        try:
            with Broker(self) as b:
                got = self.runex("send-ssl", b.port)
                self.assertIn("secure connection:", got)
                self.assertIn(send_expect(), got)
                self.assertMultiLineEqual(receive_expect(), self.runex("receive", b.port))
        except ProcError as e:
            if e.out.startswith("error initializing SSL"):
                print("Skipping %s: SSL not available" % self.id())
            else:
                raise

if __name__ == "__main__":
    unittest.main()
