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

# Extends the subprocess module to use runtime checkers, and report stderr output.

import subprocess, re, os, tempfile

from subprocess import PIPE

def in_path(name):
    """Look for name in the PATH""" 
    for path in os.environ["PATH"].split(os.pathsep):
        f = os.path.join(path, name)
        if os.path.isfile(f) and os.access(f, os.X_OK):
            return f

class TestProcessError(Exception):
    def __init__(self, proc, what, output=None):
        self.output = output
        sep = "\n%s stderr(%s) %s\n" %("_" * 32, proc.pid, "_" * 32)
        error = sep + proc.error + sep if proc.error else ""
        super(TestProcessError, self).__init__("%s pid=%s exit=%s: %s%s" % (
            proc.cmd, proc.pid, proc.returncode, what, error))

class Popen(subprocess.Popen):
    """
    Add TEST_EXE_PREFIX to the command, check stderr for runtime checker output.
    In a 'with' statement it runs check_wait() on exit from the block, or
    check_kill() if initialized with kill_me=True
    """

    def __init__(self, *args, **kwargs):
        """
        Takes all args and kwargs of subprocess.Popen except stdout, stderr, universal_newlines
        kill_me=True runs check_kill() in __exit__() instead of check_wait()
        """
        self.on_exit = self.check_kill if kwargs.pop('kill_me', False) else self.check_wait
        self.errfile = tempfile.NamedTemporaryFile(delete=False)
        kwargs.update({'universal_newlines': True, 'stdout': PIPE, 'stderr': self.errfile})
        prefix = os.environ.get("TEST_EXE_PREFIX")
        if prefix:
            args = [prefix.split() + args[0]] + list(args[1:])
        self.cmd = args[0]
        super(Popen, self).__init__(*args, **kwargs)

    def check_wait(self):
        if self.wait() or self.error:
            raise TestProcessError(self, "check_wait")

    def communicate(self, *args, **kwargs):
        result = super(Popen, self).communicate(*args, **kwargs)
        if self.returncode or self.error:
            raise TestProcessError(self, "check_communicate", result[0])
        return result

    def check_kill(self):
        """Raise if process has already exited, kill and raise if self.error is not empty"""
        if self.poll() is None:
            self.kill()
            self.wait()
            self.stdout.close()     # Doesn't get closed if killed
            if self.error:
                raise TestProcessError(self, "check_kill found error output")
        else:
            raise TestProcessError(self, "check_kill process not running")

    def expect(self, pattern):
        line = self.stdout.readline()
        match = re.search(pattern, line)
        if not match:
            raise TestProcessError(self, "can't find '%s' in '%s'" % (pattern, line))
        return match

    @property
    def error(self):
        """Return stderr as string, may only be used after process has terminated."""
        assert(self.poll is not None)
        if not hasattr(self, "_error"):
            self.errfile.close() # Not auto-deleted
            with open(self.errfile.name) as f: # Re-open to read
                self._error = f.read().strip()
            os.unlink(self.errfile.name)
        return self._error

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.on_exit()

def check_output(*args, **kwargs):
    return Popen(*args, **kwargs).communicate()[0]

class Server(Popen):
    """A process that prints 'listening on <port>' to stdout"""
    def __init__(self, *args, **kwargs):
        super(Server, self).__init__(*args, **kwargs)
        self.port = self.expect("listening on ([0-9]+)$").group(1)
