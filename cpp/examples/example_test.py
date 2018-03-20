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
from random import randrange
from subprocess import Popen, PIPE, STDOUT, call
from copy import copy
import platform
from os.path import dirname as dirname
from threading import Thread, Event
from string import Template

createdSASLDb = False

def _cyrusSetup(conf_dir):
  """Write out simple SASL config.tests
  """
  saslpasswd = os.getenv('SASLPASSWD')
  if saslpasswd:
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
_cyrusSetup('sasl-conf')

def wait_listening(proc):
    m = proc.wait_re(".*listening on ([0-9]+)$")
    return m.group(1), m.group(0)+"\n" # Return (port, line)

class BrokerTestCase(ProcTestCase):
    """
    ExampleTest that starts a broker in setUpClass and kills it in tearDownClass.
    Subclasses must set `broker_exe` class variable with the name of the broker executable.
    """
    @classmethod
    def setUpClass(cls):
        cls.broker = None       # In case Proc throws, create the attribute.
        cls.broker = Proc([cls.broker_exe, "-a", "//:0"])
        cls.port, line = wait_listening(cls.broker)
        cls.addr = "//:%s/example" % cls.port

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

def recv_expect():
    return "".join(['{"sequence"=%s}\n' % (i+1) for i in range(100)])

class ContainerExampleTest(BrokerTestCase):
    """Run the container examples, verify they behave as expected."""

    broker_exe = "broker"

    def test_helloworld(self):
        self.assertMultiLineEqual('Hello World!\n', self.proc(["helloworld", self.addr]).wait_exit())

    def test_simple_send_recv(self):
        self.assertMultiLineEqual("all messages confirmed\n", self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertMultiLineEqual(recv_expect(), self.proc(["simple_recv", "-a", self.addr]).wait_exit())

    def test_simple_recv_send(self):
        # Start receiver first, then run sender"""
        recv = self.proc(["simple_recv", "-a", self.addr])
        self.assertMultiLineEqual("all messages confirmed\n", self.proc(["simple_send", "-a", self.addr]).wait_exit())
        self.assertMultiLineEqual(recv_expect(), recv.wait_exit())


    def test_simple_send_direct_recv(self):
        recv = self.proc(["direct_recv", "-a", "//:0"])
        port, line = wait_listening(recv)
        addr = "//:%s/examples" % port
        self.assertMultiLineEqual("all messages confirmed\n",
                                  self.proc(["simple_send", "-a", addr]).wait_exit())
        self.assertMultiLineEqual(line+recv_expect(), recv.wait_exit())

    def test_simple_recv_direct_send(self):
        send = self.proc(["direct_send", "-a", "//:0"])
        port, line = wait_listening(send)
        addr = "//:%s/examples" % port
        self.assertMultiLineEqual(recv_expect(), self.proc(["simple_recv", "-a", addr]).wait_exit())
        self.assertMultiLineEqual(line+"all messages confirmed\n", send.wait_exit())

    def test_request_response(self):
        server = self.proc(["server", self.addr, "example"]) # self.addr has the connection info
        server.wait_re("connected")
        self.assertMultiLineEqual(CLIENT_EXPECT,
                         self.proc(["client", "-a", self.addr]).wait_exit())

    def test_request_response_direct(self):
        server = self.proc(["server_direct", "-a", "//:0"])
        port, line = wait_listening(server);
        addr = "//:%s/examples" % port
        self.assertMultiLineEqual(CLIENT_EXPECT, self.proc(["client", "-a", addr]).wait_exit())

    def test_flow_control(self):
        want="""success: Example 1: simple credit
success: Example 2: basic drain
success: Example 3: drain without credit
success: Example 4: high/low watermark
"""
        self.assertMultiLineEqual(want, self.proc(["flow_control", "--quiet"]).wait_exit())

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

    @unittest.skipUnless(os.getenv('HAS_CPP11'), "not a  C++11 build")
    def test_scheduled_send(self):
        out = self.proc(["scheduled_send", "-a", self.addr+"scheduled_send", "-t", "0.1", "-i", "0.001"]).wait_exit().split()
        self.assertTrue(len(out) > 0);
        self.assertEqual(["send"]*len(out), out)

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

    @unittest.skipUnless(os.getenv('HAS_CPP11'), "not a  C++11 build")
    def test_multithreaded_client(self):
        got = self.proc(["multithreaded_client", self.addr, "examples", "10"], helgrind=True).wait_exit()
        self.maxDiff = None
        self.assertRegexpMatches(got, "10 messages sent and received");

#    @unittest.skipUnless(os.getenv('HAS_CPP11'), "not a  C++11 build")
    @unittest.skip("Test is unstable, will enable when fixed")
    def test_multithreaded_client_flow_control(self):
        got = self.proc(["multithreaded_client_flow_control", self.addr, "examples", "10", "2"], helgrind=True).wait_exit()
        self.maxDiff = None
        self.assertRegexpMatches(got, "20 messages sent and received");

class ContainerExampleSSLTest(BrokerTestCase):
    """Run the SSL container examples, verify they behave as expected."""

    broker_exe = "broker"
    valgrind = False            # Disable for all tests, including inherited

    def setUp(self):
        super(ContainerExampleSSLTest, self).setUp()

    def tearDown(self):
        super(ContainerExampleSSLTest, self).tearDown()

    def ssl_certs_dir(self):
        """Absolute path to the test SSL certificates"""
        return os.path.join(dirname(sys.argv[0]), "ssl-certs")

    def test_ssl(self):
        # SSL without SASL, VERIFY_PEER_NAME
        # Disable valgrind when using OpenSSL
        out = self.proc(["ssl", "-c", self.ssl_certs_dir()]).wait_exit()
        expect = "Server certificate identity CN=test_server\nHello World!"
        self.assertIn(expect, out)

    def test_ssl_no_name(self):
        # VERIFY_PEER
        # Disable valgrind when using OpenSSL
        out = self.proc(["ssl", "-c", self.ssl_certs_dir(), "-v", "noname"], valgrind=False).wait_exit()
        expect = "Outgoing client connection connected via SSL.  Server certificate identity CN=test_server\nHello World!"
        self.assertIn(expect, out)

    def test_ssl_bad_name(self):
        # VERIFY_PEER
        out = self.proc(["ssl", "-c", self.ssl_certs_dir(), "-v", "fail"]).wait_exit()
        expect = "Expected failure of connection with wrong peer name"
        self.assertIn(expect, out)

    def test_ssl_client_cert(self):
        # SSL with SASL EXTERNAL
        expect="""Inbound client certificate identity CN=test_client
Outgoing client connection connected via SSL.  Server certificate identity CN=test_server
Hello World!
"""
        # Disable valgrind when using OpenSSL
        out = self.proc(["ssl_client_cert", self.ssl_certs_dir()]).wait_exit()
        self.assertIn(expect, out)

if __name__ == "__main__":
    unittest.main()
