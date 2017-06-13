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
import os, sys, socket, time, re, inspect
from proctest import *
from  random import randrange
from subprocess import Popen, PIPE, STDOUT, call
from copy import copy
import platform
from os.path import dirname as dirname
from threading import Thread, Event
from string import Template

createdSASLDb = False

def findfileinpath(filename, searchpath):
    """Find filename in the searchpath
        return absolute path to the file or None
    """
    paths = searchpath.split(os.pathsep)
    for path in paths:
        if os.path.exists(os.path.join(path, filename)):
            return os.path.abspath(os.path.join(path, filename))
    return None

def _cyrusSetup(conf_dir):
  """Write out simple SASL config.
  """
  saslpasswd = ""
  if 'SASLPASSWD' in os.environ:
    saslpasswd = os.environ['SASLPASSWD']
  else:
    saslpasswd = findfileinpath('saslpasswd2', os.getenv('PATH')) or ""
  if os.path.exists(saslpasswd):
    t = Template("""sasldb_path: ${db}
mech_list: EXTERNAL DIGEST-MD5 SCRAM-SHA-1 CRAM-MD5 PLAIN ANONYMOUS
""")
    abs_conf_dir = os.path.abspath(conf_dir)
    call(args=['rm','-rf',abs_conf_dir])
    os.mkdir(abs_conf_dir)
    db = os.path.join(abs_conf_dir,'proton.sasldb')
    conf = os.path.join(abs_conf_dir,'proton-server.conf')
    f = open(conf, 'w')
    f.write(t.substitute(db=db))
    f.close()

    cmd_template = Template("echo password | ${saslpasswd} -c -p -f ${db} -u proton user")
    cmd = cmd_template.substitute(db=db, saslpasswd=saslpasswd)
    call(args=cmd, shell=True)

    os.environ['PN_SASL_CONFIG_PATH'] = abs_conf_dir
    global createdSASLDb
    createdSASLDb = True

# Globally initialize Cyrus SASL configuration
#if SASL.extended():
_cyrusSetup('sasl_conf')

def ensureCanTestExtendedSASL():
#  if not SASL.extended():
#    raise Skipped('Extended SASL not supported')
  if not createdSASLDb:
    raise Skipped("Can't Test Extended SASL: Couldn't create auth db")


class BrokerTestCase(ProcTestCase):
    """
    ExampleTest that starts a broker in setUpClass and kills it in tearDownClass.
    Subclasses must set `broker_exe` class variable with the name of the broker executable.
    """

    @classmethod
    def setUpClass(cls):
        cls.broker = None       # In case Proc throws, create the attribute.
        with TestPort() as tp:
            cls.addr = "%s:%s/example" % (tp.host, tp.port)
            cls.broker = Proc([cls.broker_exe, "-a", tp.addr])
            cls.broker.wait_re("listening")

    @classmethod
    def tearDownClass(cls):
        if cls.broker:
            cls.broker.kill()

    def tearDown(self):
        b = type(self).broker
        if b and b.poll() !=  None: # Broker crashed
            type(self).setUpClass() # Start another for the next test.
            raise ProcError(b, "broker crash")
        super(BrokerTestCase, self).tearDown()


CLIENT_EXPECT="""Twas brillig, and the slithy toves => TWAS BRILLIG, AND THE SLITHY TOVES
Did gire and gymble in the wabe. => DID GIRE AND GYMBLE IN THE WABE.
All mimsy were the borogroves, => ALL MIMSY WERE THE BOROGROVES,
And the mome raths outgrabe. => AND THE MOME RATHS OUTGRABE.
"""

def recv_expect(name, addr):
    return "%s listening on %s\n%s" % (
        name, addr, "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)]))

class ContainerExampleTest(BrokerTestCase):
    """Run the container examples, verify they behave as expected."""

    broker_exe = "broker"

    def test_helloworld(self):
        self.assertMultiLineEqual('Hello World!\n', self.proc(["helloworld", self.addr]).wait_exit())

    def test_helloworld_direct(self):
        with TestPort() as tp:
            self.assertMultiLineEqual('Hello World!\n', self.proc(["helloworld_direct", tp.addr]).wait_exit())

    def test_simple_send_recv(self):
        self.assertMultiLineEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertMultiLineEqual(recv_expect("simple_recv", self.addr), self.proc(["simple_recv", "-a", self.addr]).wait_exit())

    def test_simple_recv_send(self):
        # Start receiver first, then run sender"""
        recv = self.proc(["simple_recv", "-a", self.addr])
        self.assertMultiLineEqual("all messages confirmed\n",
                         self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertMultiLineEqual(recv_expect("simple_recv", self.addr), recv.wait_exit())


    def test_simple_send_direct_recv(self):
        with TestPort() as tp:
            addr = "%s/examples" % tp.addr
            recv = self.proc(["direct_recv", "-a", addr], "listening")
            self.assertMultiLineEqual("all messages confirmed\n",
                             self.proc(["simple_send", "-a", addr]).wait_exit())
            self.assertMultiLineEqual(recv_expect("direct_recv", addr), recv.wait_exit())

    def test_simple_recv_direct_send(self):
        with TestPort() as tp:
            addr = "%s/examples" % tp.addr
            send = self.proc(["direct_send", "-a", addr], "listening")
            self.assertMultiLineEqual(recv_expect("simple_recv", addr),
                             self.proc(["simple_recv", "-a", addr]).wait_exit())
            self.assertMultiLineEqual(
                "direct_send listening on %s\nall messages confirmed\n" % addr,
                send.wait_exit())

    def test_request_response(self):
        server = self.proc(["server", "-a", self.addr], "connected")
        self.assertMultiLineEqual(CLIENT_EXPECT,
                         self.proc(["client", "-a", self.addr]).wait_exit())

    def test_request_response_direct(self):
        with TestPort() as tp:
            addr = "%s/examples" % tp.addr
            server = self.proc(["server_direct", "-a", addr], "listening")
            self.assertMultiLineEqual(CLIENT_EXPECT,
                             self.proc(["client", "-a", addr]).wait_exit())

    def test_flow_control(self):
        want="""success: Example 1: simple credit
success: Example 2: basic drain
success: Example 3: drain without credit
success: Exmaple 4: high/low watermark
"""
        with TestPort() as tp:
            self.assertMultiLineEqual(want, self.proc(["flow_control", "--address", tp.addr, "--quiet"]).wait_exit())

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
list[string(a), string(b), string(c)]

== List and map of mixed type values.
list[int(42), string(foo)]
[ 42 foo ]
map{int(4):string(four), string(five):int(5)}
{ 4:four five:5 }

== Insert with stream operators.
array<int>[int(1), int(2), int(3)]
list[int(42), boolean(0), symbol(x)]
map{string(k1):int(42), symbol(k2):boolean(0)}
"""
        self.maxDiff = None
        self.assertMultiLineEqual(want, self.proc(["encode_decode"]).wait_exit())

    def test_scheduled_send_03(self):
        # Output should be a bunch of "send" lines but can't guarantee exactly how many.
        out = self.proc(["scheduled_send_03", "-a", self.addr+"scheduled_send", "-t", "0.1", "-i", "0.001"]).wait_exit().split()
        self.assertTrue(len(out) > 0);
        self.assertEqual(["send"]*len(out), out)

    def test_scheduled_send(self):
        try:
            out = self.proc(["scheduled_send", "-a", self.addr+"scheduled_send", "-t", "0.1", "-i", "0.001"]).wait_exit().split()
            self.assertTrue(len(out) > 0);
            self.assertEqual(["send"]*len(out), out)
        except ProcError:       # File not found, not a C++11 build.
            pass

    def test_message_properties(self):
        expect="""using put/get: short=123 string=foo symbol=sym
using coerce: short(as long)=123
props[short]=123
props[string]=foo
props[symbol]=sym
short=42 string=bar
expected conversion_error: "unexpected type, want: uint got: int"
expected conversion_error: "unexpected type, want: uint got: string"
"""
        self.assertMultiLineEqual(expect, self.proc(["message_properties"]).wait_exit())



class ContainerExampleSSLTest(BrokerTestCase):
    """Run the SSL container examples, verify they behave as expected."""

    broker_exe = "broker"

    def setUp(self):
        super(ContainerExampleSSLTest, self).setUp()
        self.vg_args = Proc.vg_args
        Proc.vg_args = []       # Disable

    def tearDown(self):
        Proc.vg_args = self.vg_args
        super(ContainerExampleSSLTest, self).tearDown()

    def ssl_certs_dir(self):
        """Absolute path to the test SSL certificates"""
        pn_root = dirname(dirname(dirname(sys.argv[0])))
        return os.path.join(pn_root, "examples/cpp/ssl_certs")

    def test_ssl(self):
        # SSL without SASL, VERIFY_PEER_NAME
        with TestPort() as tp:
            addr = "amqps://%s/examples" % tp.addr
            # Disable valgrind when using OpenSSL
            out = self.proc(["ssl", "-a", addr, "-c", self.ssl_certs_dir()], skip_valgrind=True).wait_exit()
            expect = "Outgoing client connection connected via SSL.  Server certificate identity CN=test_server\nHello World!"
            expect_found = (out.find(expect) >= 0)
            self.assertEqual(expect_found, True)

    def test_ssl_no_name(self):
        # VERIFY_PEER
        with TestPort() as tp:
            addr = "amqps://%s/examples" % tp.addr
            # Disable valgrind when using OpenSSL
            out = self.proc(["ssl", "-a", addr, "-c", self.ssl_certs_dir(), "-v", "noname"], skip_valgrind=True).wait_exit()
            expect = "Outgoing client connection connected via SSL.  Server certificate identity CN=test_server\nHello World!"
            expect_found = (out.find(expect) >= 0)
            self.assertEqual(expect_found, True)

    def test_ssl_bad_name(self):
        # VERIFY_PEER
        with TestPort() as tp:
            addr = "amqps://%s/examples" % tp.addr
            # Disable valgrind when using OpenSSL
            out = self.proc(["ssl", "-a", addr, "-c", self.ssl_certs_dir(), "-v", "fail"], skip_valgrind=True).wait_exit()
            expect = "Expected failure of connection with wrong peer name"
            expect_found = (out.find(expect) >= 0)
            self.assertEqual(expect_found, True)

    def test_ssl_client_cert(self):
        # SSL with SASL EXTERNAL
        expect="""Inbound client certificate identity CN=test_client
Outgoing client connection connected via SSL.  Server certificate identity CN=test_server
Hello World!
"""
        with TestPort() as tp:
            addr = "amqps://%s/examples" % tp.addr
            # Disable valgrind when using OpenSSL
            out = self.proc(["ssl_client_cert", addr, self.ssl_certs_dir()], skip_valgrind=True).wait_exit()
            expect_found = (out.find(expect) >= 0)
            self.assertEqual(expect_found, True)

if __name__ == "__main__":
    unittest.main()
