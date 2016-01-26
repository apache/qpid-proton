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
import os, sys, socket, time, re
from  random import randrange
from subprocess import Popen, PIPE, STDOUT
from copy import copy
import platform
from os.path import dirname as dirname
from threading import Thread, Event

def pick_addr():
    """Pick a new host:port address."""
    # TODO Conway 2015-07-14: need a safer way to pick ports.
    p =  randrange(10000, 20000)
    return "127.0.0.1:%s" % p

class ProcError(Exception):
    """An exception that captures failed process output"""
    def __init__(self, proc, what="non-0 exit"):
        out = proc.out.strip()
        if out:
            out = "\nvvvvvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^^^^\n" % out
        else:
            out = ", no output)"
        super(Exception, self, ).__init__(
            "%s %s, code=%s%s" % (proc.args, what, proc.returncode, out))

class Proc(Popen):
    """A example process that stores its stdout and can scan it for a 'ready' pattern'"""

    if "VALGRIND" in os.environ and os.environ["VALGRIND"]:
        env_args = [os.environ["VALGRIND"], "--error-exitcode=42", "--quiet", "--leak-check=full"]
    else:
        env_args = []

    def __init__(self, args, ready=None, timeout=10, **kwargs):
        """Start an example process"""
        args = list(args)
        if platform.system() == "Windows":
            args[0] += ".exe"
        self.timeout = timeout
        self.args = args
        self.out = ""
        args = self.env_args + args
        try:
            Popen.__init__(self, args, stdout=PIPE, stderr=STDOUT, **kwargs)
        except Exception, e:
            raise ProcError(self, str(e))
        # Start reader thread.
        self.pattern = ready
        self.ready = Event()
        self.error = None
        self.thread = Thread(target=self.run_)
        self.thread.daemon = True
        self.thread.start()
        if self.pattern:
            self.wait_ready()

    def run_(self):
        try:
            while True:
                l = self.stdout.readline()
                if not l: break
                self.out += l.translate(None, "\r")
                if self.pattern is not None:
                    if re.search(self.pattern, l):
                        self.ready.set()
            if self.wait() != 0:
                self.error = ProcError(self)
        except Exception, e:
            self.error = sys.exc_info()
        finally:
            self.ready.set()

    def safe_kill(self):
        """Kill and clean up zombie but don't wait forever. No exceptions."""
        try:
            self.kill()
            self.thread.join(self.timeout)
        except: pass
        return self.out

    def check_(self):
        if self.error:
            if isinstance(self.error, Exception):
                raise self.error
            raise self.error[0], self.error[1], self.error[2] # with traceback

    def wait_ready(self):
        """Wait for ready to appear in output"""
        if self.ready.wait(self.timeout):
            self.check_()
            return self.out
        else:
            self.safe_kill()
            raise ProcError(self, "timeout waiting for '%s'" % self.pattern)

    def wait_exit(self):
        """Wait for process to exit, return output. Raise ProcError on failure."""
        self.thread.join(self.timeout)
        if self.poll() is not None:
            self.check_()
            return self.out
        else:
            raise ProcError(self, "timeout waiting for exit")


class ExampleTestCase(unittest.TestCase):
    def setUp(self):
        self.procs = []

    def tearDown(self):
        for p in self.procs:
            p.safe_kill()

    def proc(self, *args, **kwargs):
        p = Proc(*args, **kwargs)
        self.procs.append(p)
        return p


class BrokerTestCase(ExampleTestCase):
    """
    ExampleTest that starts a broker in setUpClass and kills it in tearDownClass.
    """

    @classmethod
    def setUpClass(cls):
        cls.addr = pick_addr() + "/examples"
        cls.broker = Proc(["broker", "-a", cls.addr], ready="listening")
        cls.broker.wait_ready()

    @classmethod
    def tearDownClass(cls):
        cls.broker.safe_kill()

    def tearDown(self):
        super(BrokerTestCase, self).tearDown()
        b = type(self).broker
        if b.poll() !=  None: # Broker crashed
            type(self).setUpClass() # Start another for the next test.
            raise ProcError(b, "broker crash")


CLIENT_EXPECT="""Twas brillig, and the slithy toves => TWAS BRILLIG, AND THE SLITHY TOVES
Did gire and gymble in the wabe. => DID GIRE AND GYMBLE IN THE WABE.
All mimsy were the borogroves, => ALL MIMSY WERE THE BOROGROVES,
And the mome raths outgrabe. => AND THE MOME RATHS OUTGRABE.
"""

def recv_expect(name, addr):
    return "%s listening on amqp://%s\n%s" % (
        name, addr, "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)]))

class ContainerExampleTest(BrokerTestCase):
    """Run the container examples, verify they behave as expected."""

    def test_helloworld(self):
        self.assertEqual('Hello World!\n', self.proc(["helloworld", self.addr]).wait_exit())

    def test_helloworld_direct(self):
        self.assertEqual('Hello World!\n', self.proc(["helloworld_direct", pick_addr()]).wait_exit())

    def test_simple_send_recv(self):
        self.assertEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertEqual(recv_expect("simple_recv", self.addr), self.proc(["simple_recv", "-a", self.addr]).wait_exit())

    def test_simple_recv_send(self):
        # Start receiver first, then run sender"""
        recv = self.proc(["simple_recv", "-a", self.addr])
        self.assertEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertEqual(recv_expect("simple_recv", self.addr), recv.wait_exit())


    def test_simple_send_direct_recv(self):
        addr = pick_addr()
        recv = self.proc(["direct_recv", "-a", addr], "listening")
        self.assertEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", addr]).wait_exit())
        self.assertEqual(recv_expect("direct_recv", addr), recv.wait_exit())

    def test_simple_recv_direct_send(self):
        addr = pick_addr()
        send = self.proc(["direct_send", "-a", addr], "listening")
        self.assertEqual(recv_expect("simple_recv", addr),
                         self.proc(["simple_recv", "-a", addr]).wait_exit())

        self.assertEqual(
            "direct_send listening on amqp://%s\nall messages confirmed\n" % addr,
            send.wait_exit())

    def test_request_response(self):
        server = self.proc(["server", "-a", self.addr], "connected")
        self.assertEqual(CLIENT_EXPECT,
                         self.proc(["client", "-a", self.addr]).wait_exit())

    def test_request_response_direct(self):
        addr = pick_addr()
        server = self.proc(["server_direct", "-a", addr+"/examples"], "listening")
        self.assertEqual(CLIENT_EXPECT,
                         self.proc(["client", "-a", addr+"/examples"]).wait_exit())

    def test_encode_decode(self):
        want="""
== Array, list and map of uniform type.
array<int>[int(1), int(2), int(3)]
[ 1 2 3 ]
list[int(1), int(2), int(3)]
[ 1 2 3 ]
map{string(one):int(1), string(two):int(2)}
{ one:1 two:2 }
map{string(z):int(3), string(a):int(4)}
[ z:3 a:4 ]

== List and map of mixed type values.
list[int(42), string(foo)]
[ 42 foo ]
map{int(4):string(four), string(five):int(5)}
{ 4:four five:5 }

== Insert with stream operators.
array<int>[int(1), int(2), int(3)]
list[int(42), boolean(false), symbol(x)]
map{string(k1):int(42), symbol(k2):boolean(false)}
"""
        self.maxDiff = None
        self.assertEqual(want, self.proc(["encode_decode"]).wait_exit())

    def test_recurring_timer(self):
        env = copy(os.environ)        # Disable valgrind, this test is time-sensitive.
        if "VALGRIND" in os.environ:
            del os.environ["VALGRIND"]
        try:
            expect="""Tick...
Tick...
Tock...
"""
            self.maxDiff = None
            self.assertEqual(expect, self.proc(["recurring_timer", "-t", ".05", "-k", ".01"]).wait_exit())
        finally:
            os.environ = env    # Restore environment

    def ssl_certs_dir(self):
        """Absolute path to the test SSL certificates"""
        pn_root = dirname(dirname(dirname(sys.argv[0])))
        return os.path.join(pn_root, "examples/cpp/ssl_certs")

    def test_ssl(self):
        # SSL without SASL
        addr = "amqps://" + pick_addr() + "/examples"
        out = self.proc(["ssl", addr, self.ssl_certs_dir()]).wait_exit()
        expect = "Outgoing client connection connected via SSL.  Server certificate identity CN=test_server\nHello World!"
        self.assertIn(expect, out)


    def test_ssl_client_cert(self):
        # SSL with SASL EXTERNAL
        expect="""Inbound client certificate identity CN=test_client
Outgoing client connection connected via SSL.  Server certificate identity CN=test_server
Hello World!
"""
        addr = "amqps://" + pick_addr() + "/examples"
        out = self.proc(["ssl_client_cert", addr, self.ssl_certs_dir()]).wait_exit()
        self.assertIn(expect, out)


class ConnectionEngineExampleTest(BrokerTestCase):
    """Run the connction_engine examples, verify they behave as expected."""

    def test_helloworld(self):
        self.assertEqual('Hello World!\n',
                         self.proc(["helloworld", self.addr]).wait_exit())

    def test_simple_send_recv(self):
        self.assertEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertEqual(recv_expect("simple_recv", self.addr), self.proc(["simple_recv", "-a", self.addr]).wait_exit())

    def test_simple_recv_send(self):
        # Start receiver first, then run sender"""
        recv = self.proc(["simple_recv", "-a", self.addr])
        self.assertEqual("all messages confirmed\n", self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertEqual(recv_expect("simple_recv", self.addr), recv.wait_exit())


    def test_simple_send_direct_recv(self):
        addr = pick_addr()
        recv = self.proc(["direct_recv", "-a", addr], "listening")
        self.assertEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", addr]).wait_exit())
        self.assertEqual(recv_expect("direct_recv", addr), recv.wait_exit())

    def test_simple_recv_direct_send(self):
        addr = pick_addr()
        send = self.proc(["direct_send", "-a", addr], "listening")
        self.assertEqual(recv_expect("simple_recv", addr),
                         self.proc(["simple_recv", "-a", addr]).wait_exit())
        self.assertEqual("direct_send listening on amqp://%s\nall messages confirmed\n" % addr,
                         send.wait_exit())

    def test_request_response(self):
        server = self.proc(["server", "-a", self.addr], "connected")
        self.assertEqual(CLIENT_EXPECT,
                         self.proc(["client", "-a", self.addr]).wait_exit())

if __name__ == "__main__":
    unittest.main()
