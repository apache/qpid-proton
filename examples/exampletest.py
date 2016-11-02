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

# A test library to make it easy to run unittest tests that start,
# monitor, and report output from sub-processes. In particular
# it helps with starting processes that listen on random ports.

import unittest
import os, sys, socket, time, re, inspect, errno, threading
from  random import randrange
from subprocess import Popen, PIPE, STDOUT
from copy import copy
import platform
from os.path import dirname as dirname

def pick_port():
    """Pick a random port."""
    p =  randrange(10000, 20000)
    return p

class ProcError(Exception):
    """An exception that captures failed process output"""
    def __init__(self, proc, what="bad exit status"):
        out = proc.out.strip()
        if out:
            out = "\nvvvvvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^^^^\n" % out
        else:
            out = ", no output)"
        super(Exception, self, ).__init__(
            "%s %s, code=%s%s" % (proc.args, what, proc.returncode, out))

class NotFoundError(ProcError):
    pass

class Proc(Popen):
    """A example process that stores its output, optionally run with valgrind."""

    if "VALGRIND" in os.environ and os.environ["VALGRIND"]:
        env_args = [os.environ["VALGRIND"], "--error-exitcode=42", "--quiet", "--leak-check=full"]
    else:
        env_args = []

    @property
    def out(self):
        self._out.seek(0)
        return self._out.read()

    def __init__(self, args, **kwargs):
        """Start an example process"""
        args = list(args)
        self.args = args
        self._out = os.tmpfile()
        try:
            Popen.__init__(self, self.env_args + self.args, stdout=self._out, stderr=STDOUT, **kwargs)
        except OSError, e:
            if e.errno == errno.ENOENT:
                raise NotFoundError(self, str(e))
            raise ProcError(self, str(e))
        except Exception, e:
            raise ProcError(self, str(e))

    def kill(self):
        try:
            if self.poll() is None:
                Popen.kill(self)
        except:
            pass                # Already exited.
        return self.out

    def wait_out(self, timeout=10, expect=0):
        """Wait for process to exit, return output. Raise ProcError  on failure."""
        t = threading.Thread(target=self.wait)
        t.start()
        t.join(timeout)
        if self.poll() is None:      # Still running
            self.kill()
            raise ProcError(self, "timeout")
        if expect is not None and self.poll() != expect:
            raise ProcError(self)
        return self.out

# Work-around older python unittest that lacks setUpClass.
if hasattr(unittest.TestCase, 'setUpClass') and  hasattr(unittest.TestCase, 'tearDownClass'):
    TestCase = unittest.TestCase
else:
    class TestCase(unittest.TestCase):
        """
        Roughly provides setUpClass and tearDownClass functionality for older python
        versions in our test scenarios. If subclasses override setUp or tearDown
        they *must* call the superclass.
        """
        def setUp(self):
            if not hasattr(type(self), '_setup_class_count'):
                type(self)._setup_class_count = len(
                    inspect.getmembers(
                        type(self),
                        predicate=lambda(m): inspect.ismethod(m) and m.__name__.startswith('test_')))
                type(self).setUpClass()

        def tearDown(self):
            self.assertTrue(self._setup_class_count > 0)
            self._setup_class_count -=  1
            if self._setup_class_count == 0:
                type(self).tearDownClass()

class ExampleTestCase(TestCase):
    """TestCase that manages started processes"""
    def setUp(self):
        super(ExampleTestCase, self).setUp()
        self.procs = []

    def tearDown(self):
        for p in self.procs:
            p.kill()
        super(ExampleTestCase, self).tearDown()

    def proc(self, *args, **kwargs):
        p = Proc(*args, **kwargs)
        self.procs.append(p)
        return p

def wait_port(port, timeout=10):
    """Wait up to timeout for port to be connectable."""
    if timeout:
        deadline = time.time() + timeout
    while (timeout is None or time.time() < deadline):
        try:
            s = socket.create_connection((None, port), timeout) # Works for IPv6 and v4
            s.close()
            return
        except socket.error, e:
            if e.errno != errno.ECONNREFUSED: # Only retry on connection refused error.
                raise
    raise socket.timeout()


class BrokerTestCase(ExampleTestCase):
    """
    ExampleTest that starts a broker in setUpClass and kills it in tearDownClass.
    Subclass must set `broker_exe` class variable with the name of the broker executable.
    """

    @classmethod
    def setUpClass(cls):
        cls.port = pick_port()
        cls.addr = "127.0.0.1:%s/examples" % (cls.port)
        cls.broker = None       # In case Proc throws, create the attribute.
        cls.broker = Proc(cls.broker_exe + ["-a", cls.addr])
        try:
            wait_port(cls.port)
        except Exception, e:
            cls.broker.kill()
            raise ProcError(cls.broker, "timed out waiting for port")

    @classmethod
    def tearDownClass(cls):
        if cls.broker: cls.broker.kill()

    def tearDown(self):
        b = type(self).broker
        if b and b.poll() !=  None: # Broker crashed
            type(self).setUpClass() # Start another for the next test.
            raise ProcError(b, "broker crash")
        super(BrokerTestCase, self).tearDown()

if __name__ == "__main__":
    unittest.main()
