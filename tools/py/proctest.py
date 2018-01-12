
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

class ProcError(Exception):
    """An exception that displays failed process output"""
    def __init__(self, proc, what="bad exit status"):
        self.out = proc.out.strip()
        returncode = getattr(proc, 'returncode') # Can be missing in some cases
        msg = "%s (exit=%s) command:\n%s" % (what, returncode, " ".join(proc.args))
        if self.out:
            msg += "\nvvvvvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^^^^" % self.out
        else:
            msg += "\n<<no output>>"
        super(ProcError, self, ).__init__(msg)

class NotFoundError(ProcError):
    pass

class Proc(Popen):
    """Subclass of suprocess.Popen that stores its output and can scan it for a
    'ready' pattern' Use self.out to access output (combined stdout and stderr).
    You can't set the Popen stdout and stderr arguments, they will be overwritten.
    """

    @property
    def out(self):
        self._out.seek(0)
        return self._out.read()

    def __init__(self, args, valgrind=True, helgrind=False, **kwargs):
        """Start an example process"""
        self.args = list(args)
        self.kwargs = kwargs
        self._out = tempfile.TemporaryFile(mode='w+')
        valgrind_exe = valgrind and os.getenv("VALGRIND")
        if valgrind_exe:
            # run valgrind for speed, not for detailed information
            vg = [valgrind_exe]
            if helgrind:
                vg += ["--tool=helgrind", "--quiet", "--error-exitcode=42"]
            else:
                vg += ["--tool=memcheck"] + os.getenv("VALGRIND_ARGS").split(' ')
            self.args = vg + self.args
        if os.getenv("PROCTEST_VERBOSE"):
            sys.stderr.write("\n== running == "+" ".join(self.args)+"\n")
        try:
            Popen.__init__(self, self.args, stdout=self._out, stderr=STDOUT, **kwargs)
        except OSError as e:
            if e.errno == errno.ENOENT:
                raise NotFoundError(self, str(e))
            raise ProcError(self, str(e))
        except Exception as e:
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

    # Default value for valgrind= in proc() function if not explicitly set.
    # Override by setting a "valgrind" member in subclass or instance.
    valgrind=True

    def proc(self, *args, **kwargs):
        """Return a Proc() that will be automatically killed on teardown"""
        if 'valgrind' in kwargs:
            p = Proc(*args, **kwargs)
        else:
            p = Proc(*args, valgrind=self.valgrind, **kwargs)
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
            self.procs = []

        def tearDown(self):
            self.assertTrue(self._setup_class_count > 0)
            self._setup_class_count -=  1
            if self._setup_class_count == 0:
                type(self).tearDownClass()
            for p in self.procs:
                p.kill()
            super(ProcTestCase, self).tearDown()

    if _tc_missing('assertIn'):
        def assertIn(self, a, b):
            self.assertTrue(a in b, "%r not in %r" % (a, b))

    if _tc_missing('assertMultiLineEqual'):
        def assertMultiLineEqual(self, a, b):
            self.assertEqual(a, b)

from functools import reduce

#### Skip decorators missing in python 2.6

def _id(obj):
    return obj

from functools import wraps

def skip(reason):
    def decorator(test):
       @wraps(test)
       def skipper(*args, **kwargs):
           print("skipped %s: %s" % (test.__name__, reason))
       return skipper
    return decorator

def skipIf(cond, reason):
    if cond: return skip(reason)
    else: return _id

def skipUnless(cond, reason):
    if not cond: return skip(reason)
    else: return _id

if not hasattr(unittest, 'skip'): unittest.skip = skip
if not hasattr(unittest, 'skipIf'): unittest.skipIf = skipIf
if not hasattr(unittest, 'skipUnless'): unittest.skipUnless = skipUnless

from unittest import main
if __name__ == "__main__":
    main()
