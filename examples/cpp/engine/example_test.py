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
from copy import copy
import platform
from os.path import dirname as dirname

def cmdline(*args):
    """Adjust executable name args[0] for windows and/or valgrind"""
    args = list(args)
    if platform.system() == "Windows":
        args[0] += ".exe"
    if "VALGRIND" in os.environ and os.environ["VALGRIND"]:
        args = [os.environ["VALGRIND"], "--error-exitcode=42", "--quiet",
                "--leak-check=full"] + args
    return args

def background(*args):
    """Run executable in the backround, return the popen"""
    p = Popen(cmdline(*args), stdout=PIPE, stderr=sys.stderr)
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
        if out:
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
    # TODO aconway 2015-07-14: need a safer way to pick ports.
    p =  randrange(10000, 20000)
    return "127.0.0.1:%s" % p

def ssl_certs_dir():
    """Absolute path to the test SSL certificates"""
    pn_root = dirname(dirname(dirname(sys.argv[0])))
    return os.path.join(pn_root, "examples/cpp/ssl_certs")

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
        broker_exe = os.environ.get("TEST_BROKER") or "broker"
        self.addr = pick_addr()
        cmd = cmdline(broker_exe, "-a", self.addr)
        try:
            self.process = Popen(cmd, stdout=NULL, stderr=sys.stderr)
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
        self.assertEqual('Hello World!\n', hw)

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
        while not "listening" in recv.stdout.readline():
            pass
        self.assertEqual("all messages confirmed\n", execute("simple_send", "-a", addr))
        recv_expect = "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, verify(recv))

    def test_simple_recv_direct_send(self):
        addr = pick_addr()
        send = background("direct_send", "-a", addr)
        while not "listening" in send.stdout.readline():
            pass
        recv_expect = "simple_recv listening on amqp://%s\n" % (addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, execute("simple_recv", "-a", addr))
        send_expect = "all messages confirmed\n"
        self.assertEqual(send_expect, verify(send))

    CLIENT_EXPECT="""Twas brillig, and the slithy toves => TWAS BRILLIG, AND THE SLITHY TOVES
Did gire and gymble in the wabe. => DID GIRE AND GYMBLE IN THE WABE.
All mimsy were the borogroves, => ALL MIMSY WERE THE BOROGROVES,
And the mome raths outgrabe. => AND THE MOME RATHS OUTGRABE.
"""
    def test_simple_recv_send(self):
        # Start receiver first, then run sender"""
        b = Broker.get()
        recv = background("simple_recv", "-a", b.addr)
        self.assertEqual("all messages confirmed\n", execute("simple_send", "-a", b.addr))
        recv_expect = "simple_recv listening on amqp://%s\n" % (b.addr)
        recv_expect += "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])
        self.assertEqual(recv_expect, verify(recv))

    def test_client_server(self):
        b = Broker.get()
        server = background("server", "-a", b.addr)
        try:
            self.assertEqual(execute("client", "-a", b.addr), self.CLIENT_EXPECT)
        finally:
            server.kill()

if __name__ == "__main__":
    unittest.main()
