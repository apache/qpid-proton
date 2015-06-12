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

import unittest
import os, sys, socket, time
from  random import randrange
from subprocess import Popen, check_output, PIPE, STDOUT

NULL = open(os.devnull, 'w')

class Broker(object):
    """Run the test broker"""

    @classmethod
    def get(cls):
        if not hasattr(cls, "_broker"):
            cls._broker = Broker()
        return cls._broker

    @classmethod
    def stop(cls):
        if cls._broker and cls._broker.process:
            cls._broker.process.kill()
            cls._broker = None

    def __init__(self):
        self.port = randrange(10000, 20000)
        self.addr = ":%s" % self.port
        self.process = Popen(["./broker", self.addr], stdout=NULL, stderr=NULL)
        # Wait 10 secs for broker to listen
        deadline = time.time() + 10
        c = None
        while time.time() < deadline:
            try:
                c = socket.create_connection(("127.0.0.1", self.port), deadline - time.time())
                break
            except socket.error as e:
                time.sleep(0.01)
        if c is None:
            raise Exception("Timed out waiting for broker")
        c.close()


class ExampleTest(unittest.TestCase):
    """Run the examples, verify they behave as expected."""

    @classmethod
    def tearDownClass(self):
        Broker.stop()

    def test_helloworld(self):
        b = Broker.get()
        hw = check_output(["./helloworld", b.addr])
        self.assertEqual("Hello World!\n", hw)

    def test_helloworld_blocking(self):
        b = Broker.get()
        hw = check_output(["./helloworld_blocking", b.addr])
        self.assertEqual("Hello World!\n", hw)

    def test_helloworld_direct(self):
        url = ":%s/examples" % randrange(10000, 20000)
        hw = check_output(["./helloworld_direct", url])
        self.assertEqual("Hello World!\n", hw)

    def test_simple_send_recv(self):
        b = Broker.get()
        n = 5
        send = check_output(["./simple_send", "-a", b.addr, "-m", str(n)])
        self.assertEqual("all messages confirmed\n", send)
        recv = check_output(["./simple_recv", "-a", b.addr, "-m", str(n)])
        recv_expect = "simple_recv listening on %s\n" % (b.addr)
        recv_expect += "".join(['[%d]: b"some arbitrary binary data"\n' % (i+1) for i in range(n)])
        self.assertEqual(recv_expect, recv)

        # FIXME aconway 2015-06-16: bug when receiver is started before sender, messages
        # are not delivered to receiver.
    def FIXME_test_simple_recv_send(self):
        """Start receiver first, then run sender"""
        b = Broker.get()
        n = 5
        recv = Popen(["./simple_recv", "-a", b.addr, "-m", str(n)], stdout=PIPE)
        self.assertEqual("simple_recv listening on %s\n" % (b.addr), recv.stdout.readline())
        send = check_output(["./simple_send", "-a", b.addr, "-m", str(n)])
        self.assertEqual("all messages confirmed\n", send)
        recv_expect = "".join(['[%d]: b"some arbitrary binary data"\n' % (i+1) for i in range(n)])
        out, err = recv.communicate()
        self.assertEqual(recv_expect, out)

    def call(self, *cmd):
        p = Popen(cmd, stdout=PIPE, stderr=STDOUT)
        out, err = p.communicate()
        self.assertEqual(0, p.returncode,
                         "%s exit code %s, output:\n%s\n---- end of %s exit code %s" % (
                             cmd, p.returncode, out, cmd, p.returncode))
        return out

    def test_encode_decode(self):
        expect="""
== Simple values: int, string, bool
Values: int(42), string("foo"), bool(true)
Extracted: 42, foo, 1
Encoded as AMQP in 8 bytes

== Specific AMQP types: byte, long, symbol
Values: byte(120), long(123456789123456789), symbol(:bar)
Extracted (with conversion) 120, 123456789123456789, bar
Extracted (exact) x, 123456789123456789, bar

== Array, list and map.
Values: array<int>[int(1), int(2), int(3)], list[int(4), int(5)], map{string("one"):int(1), string("two"):int(2)}
Extracted: [ 1 2 3 ], [ 4 5 ], { one:1 two:2 }

== List and map of mixed type values.
Values: list[int(42), string("foo")], map{int(4):string("four"), string("five"):int(5)}
Extracted: [ 42 "foo" ], { 4:"four" "five":5 }

== Insert with stream operators.
Values: array<int>[int(1), int(2), int(3)]
Values: list[int(42), bool(false), symbol(:x)]
Values: map{string("k1"):int(42), symbol(:"k2"):bool(false)}
"""
        self.maxDiff = None
        self.assertMultiLineEqual(expect, self.call("./encode_decode"))
if __name__ == "__main__":
    unittest.main()
