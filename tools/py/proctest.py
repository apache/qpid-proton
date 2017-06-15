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

"""Unit test library to simplify tests that start, monitor, check and report
output from sub-processes. Provides safe port allocation for processes that
listen on a port. Allows executables to be run under a debugging tool like
valgrind.
"""

import unittest
import os, sys, socket, time, re, inspect, errno, threading, tempfile
from  random import randrange
from subprocess import Popen, PIPE, STDOUT
from copy import copy
import platform
from os.path import dirname as dirname

DEFAULT_TIMEOUT=10

class TestPort(object):
    """Get an unused port using bind(0) and SO_REUSEADDR and hold it till close()
    Can be used as `with TestPort() as tp:` Provides tp.host, tp.port and tp.addr
    (a "host:port" string)
    """
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('127.0.0.1', 0)) # Testing exampless is local only
        self.host, self.port = socket.getnameinfo(self.sock.getsockname(), 0)
        self.addr = "%s:%s" % (self.host, self.port)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def close(self):
        self.sock.close()

class ProcError(Exception):
    """An exception that displays failed process output"""
    def __init__(self, proc, what="bad exit status"):
        self.out = proc.out.strip()
        if self.out:
            msgtail = "\nvvvvvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^^^^\n" % self.out
        else:
            msgtail = ", no output"
        super(Exception, self, ).__init__(
            "%s %s, code=%s%s" % (proc.args, what, getattr(proc, 'returncode', 'noreturn'), msgtail))

class NotFoundError(ProcError):
    pass

class Proc(Popen):
    """Subclass of suprocess.Popen that stores its output and can scan it for a
    'ready' pattern' Use self.out to access output (combined stdout and stderr).
    You can't set the Popen stdout and stderr arguments, they will be overwritten.
    """

    if "VALGRIND" in os.environ and os.environ["VALGRIND"]:
        vg_args = [os.environ["VALGRIND"], "--error-exitcode=42", "--quiet", "--leak-check=full"]
    else:
        vg_args = []

    @property
    def out(self):
        self._out.seek(0)
        # Normalize line endings, os.tmpfile() opens in binary mode.
        return self._out.read().replace('\r\n','\n').replace('\r','\n')

    def __init__(self, args, skip_valgrind=False, **kwargs):
        """Start an example process"""
        args = list(args)
        if skip_valgrind:
            self.args = args
        else:
            self.args = self.vg_args + args
        self.kwargs = kwargs
        self._out = tempfile.TemporaryFile()
        try:
            Popen.__init__(self, self.args, stdout=self._out, stderr=STDOUT, **kwargs)
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

    def wait_exit(self, timeout=DEFAULT_TIMEOUT, expect=0):
        """Wait for process to exit, return output. Raise ProcError  on failure."""
        t = threading.Thread(target=self.wait)
        t.start()
        t.join(timeout)
        if self.poll() is None:      # Still running
            self.kill()
            raise ProcError(self, "still running after %ss" % timeout)
        if expect is not None and self.poll() != expect:
            raise ProcError(self)
        return self.out

    def wait_re(self, regexp, timeout=DEFAULT_TIMEOUT):
        """
        Wait for regexp to appear in the output, returns the re.search match result.
        The target process should flush() important output to ensure it appears.
        """
        if timeout:
            deadline = time.time() + timeout
        while timeout is None or time.time() < deadline:
            match = re.search(regexp, self.out)
            if match:
                return match
            if self.poll() is not None:
                raise ProcError(self, "process exited while waiting for '%s'" % (regexp))
            time.sleep(0.01)    # Not very efficient
        raise ProcError(self, "gave up waiting for '%s' after %ss" % (regexp, timeout))

def _tc_missing(attr):
    return not hasattr(unittest.TestCase, attr)

class ProcTestCase(unittest.TestCase):
    """TestCase that manages started processes

    Also roughly provides setUpClass() and tearDownClass() and other features
    missing in python 2.6. If subclasses override setUp() or tearDown() they
    *must* call the superclass.
    """

    def setUp(self):
        super(ProcTestCase, self).setUp()
        self.procs = []

    def tearDown(self):
        for p in self.procs:
            p.kill()
        super(ProcTestCase, self).tearDown()

    def proc(self, *args, **kwargs):
        """Return a Proc() that will be automatically killed on teardown"""
        p = Proc(*args, **kwargs)
        self.procs.append(p)
        return p

    if _tc_missing('setUpClass') and _tc_missing('tearDownClass'):

        @classmethod
        def setUpClass(cls):
            pass

        @classmethod
        def tearDownClass(cls):
            pass

        def setUp(self):
            super(ProcTestCase, self).setUp()
            cls = type(self)
            if not hasattr(cls, '_setup_class_count'): # First time
                def is_test(m):
                    return inspect.ismethod(m) and m.__name__.startswith('test_')
                cls._setup_class_count = len(inspect.getmembers(cls, predicate=is_test))
                cls.setUpClass()

        def tearDown(self):
            self.assertTrue(self._setup_class_count > 0)
            self._setup_class_count -=  1
            if self._setup_class_count == 0:
                type(self).tearDownClass()
            super(ProcTestCase, self).tearDown()

    if _tc_missing('assertIn'):
        def assertIn(self, a, b):
            self.assertTrue(a in b, "%r not in %r" % (a, b))

    if _tc_missing('assertMultiLineEqual'):
        def assertMultiLineEqual(self, a, b):
            self.assertEqual(a, b)

from unittest import main
if __name__ == "__main__":
    main()
