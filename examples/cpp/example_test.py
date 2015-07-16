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

import unittest
import os, sys, socket, time
from  random import randrange
from subprocess import Popen, PIPE, STDOUT
import platform

def exe_name(name):
    if platform.system() == "Windows":
        return name + ".exe"
    return "./" + name

def background(*args):
    """Run executable in the backround, return the popen"""
    args = [exe_name(args[0])]+list(args[1:])
    p = Popen(args, stdout=PIPE, stderr=STDOUT)
    p.args = args               # Save arguments for debugging output
    return p

def verify(p):
    """Wait for executable to exit and verify status."""
    try:
        out, err = p.communicate()
    except Exception as e:
        raise Exception("Error running %s: %s", p.args, e)
    if p.returncode:
        raise Exception("""%s exit code %s
vvvvvvvvvvvvvvvv
%s
^^^^^^^^^^^^^^^^
""" % (p.args, p.returncode, out))
    if platform.system() == "Windows":
        # Just \n please
        out = out.translate(None, '\r')
    return out

def execute(*args):
    return verify(background(*args))

NULL = open(os.devnull, 'w')

def wait_addr(addr, timeout=10):
    """Wait up to timeout for something to listen on port"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            c = socket.create_connection(addr.split(":"), deadline - time.time())
            c.close()
            return
        except socket.error as e:
            time.sleep(0.01)
    raise Exception("Timed out waiting for %s", addr)

def pick_addr():
    """Pick a new host:port address."""
    # FIXME aconway 2015-07-14: need a safer way to pick ports.
    p =  randrange(10000, 20000)
    return "127.0.0.1:%s" % p

class Broker(object):
    """Run the test broker"""

    @classmethod
    def get(cls):
        if not hasattr(cls, "_broker"):
            cls._broker = Broker()
        return cls._broker

    @classmethod
    def stop(cls):
        if cls.get() and cls._broker.process:
            cls._broker.process.kill()
            cls._broker = None

    def __init__(self):
        self.addr = pick_addr()
        cmd = [exe_name("broker"), "-a", self.addr]
        try:
            self.process = Popen(cmd, stdout=NULL, stderr=NULL)
            wait_addr(self.addr)
            self.addr += "/examples"
        except Exception as e:
            raise Exception("Error running %s: %s", cmd, e)

class ExampleTest(unittest.TestCase):
    """Run the examples, verify they behave as expected."""

    @classmethod
    def tearDownClass(self):
        Broker.stop()

    def test_helloworld(self):
        b = Broker.get()
        hw = execute("helloworld", b.addr)
        self.assertEqual('"Hello World!"\n', hw)

    def test_helloworld_blocking(self):
        b = Broker.get()
        hw = execute("helloworld_blocking", b.addr, b.addr)
        self.assertEqual('"Hello World!"\n', hw)

    def test_helloworld_direct(self):
        addr = pick_addr()
        hw = execute("helloworld_direct", addr)
        self.assertEqual('"Hello World!"\n', hw)

    def test_simple_send_recv(self):
        b = Broker.get()
        send = execute("simple_send", "-a", b.addr)
        self.assertEqual("all messages confirmed\n", send)
        recv = execute("simple_recv", "-a", b.addr)
        recv_expect = "simple_recv listening on amqp://%s\n" % (b.addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, recv)

    def test_simple_send_direct_recv(self):
        addr = pick_addr()
        recv = background("direct_recv", "-a", addr)
        wait_addr(addr)
        self.assertEqual("all messages confirmed\n", execute("simple_send", "-a", addr))
        recv_expect = "direct_recv listening on amqp://%s\n" % (addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, verify(recv))

    def test_simple_recv_direct_send(self):
        addr = pick_addr()
        send = background("direct_send", "-a", addr)
        wait_addr(addr)
        recv_expect = "simple_recv listening on amqp://%s\n" % (addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, execute("simple_recv", "-a", addr))
        send_expect = "direct_send listening on amqp://%s\nall messages confirmed\n" % (addr)
        self.assertEqual(send_expect, verify(send))

    def test_simple_recv_send(self):
        """Start receiver first, then run sender"""
        b = Broker.get()
        recv = background("simple_recv", "-a", b.addr)
        self.assertEqual("all messages confirmed\n", execute("simple_send", "-a", b.addr))
        recv_expect = "simple_recv listening on amqp://%s\n" % (b.addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, verify(recv))

    def test_encode_decode(self):
        expect="""
== Simple values: int, string, bool
Values: int(42), string("foo"), bool(true)
Extracted: 42, foo, 1
Encoded as AMQP in 8 bytes
Decoded: 42, foo, 1

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
        self.assertEqual(expect, execute("encode_decode"))
if __name__ == "__main__":
    unittest.main()
