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
# under the License.
#

# TODO: summarize, test harness preconditions (e.g. broker is alive)

import optparse
import os
import struct
import sys
import time
import traceback
import types
import html
from fnmatch import fnmatchcase as match
from logging import getLogger, StreamHandler, Formatter, Filter, \
    WARN, DEBUG, ERROR, INFO

from .common import SkipTest

levels = {
    "DEBUG": DEBUG,
    "INFO": INFO,
    "WARN": WARN,
    "ERROR": ERROR
}

sorted_levels = [k for k, v in sorted(levels.items(), key=lambda kv: kv[1])]

parser = optparse.OptionParser(usage="usage: %prog [options] PATTERN ...",
                               description="Run tests matching the specified PATTERNs.")
parser.add_option("-l", "--list", action="store_true", default=False,
                  help="list tests instead of executing them")
parser.add_option("-f", "--log-file", metavar="FILE", help="log output to FILE")
parser.add_option("-v", "--log-level", metavar="LEVEL", default="WARN",
                  help="only display log messages of LEVEL or higher severity: "
                  "%s (default %%default)" % ", ".join(sorted_levels))
parser.add_option("-c", "--log-category", metavar="CATEGORY", action="append",
                  dest="log_categories", default=[],
                  help="log only categories matching CATEGORY pattern")
parser.add_option("-m", "--module", action="append", default=[],
                  dest="modules", help="add module to test search path")
parser.add_option("-i", "--ignore", action="append", default=[],
                  help="ignore tests matching IGNORE pattern")
parser.add_option("-I", "--ignore-file", metavar="IFILE", action="append",
                  default=[],
                  help="ignore tests matching patterns in IFILE")
parser.add_option("-H", "--halt-on-error", action="store_true", default=False,
                  dest="hoe", help="halt if an error is encountered")
parser.add_option("-t", "--time", action="store_true", default=False,
                  help="report timing information on test run")
parser.add_option("-D", "--define", metavar="DEFINE", dest="defines",
                  action="append", default=[], help="define test parameters")
parser.add_option("-x", "--xml", metavar="XML", dest="xml",
                  help="write test results in Junit style xml suitable for use by CI tools etc")
parser.add_option("-a", "--always-colorize", action="store_true", dest="always_colorize", default=False,
                  help="always colorize the test results rather than relying on terminal tty detection. "
                  "Useful when invoked from Jython/Maven.")
parser.add_option("-n", metavar="count", dest="count", type=int, default=1,
                  help="run the tests <count> times")
parser.add_option("-b", "--bare", action="store_true", default=False,
                  help="Run bare, i.e. don't capture stack traces. This is useful under Jython as "
                  "captured stack traces do not include the Java portion of the stack,"
                  "whereas non captured stack traces do.")
parser.add_option("-j", "--javatrace", action="store_true", default=False,
                  help="Show the full java stack trace. This disables heuristics to eliminate the "
                  "jython portion of java stack traces.")


class Config:

    def __init__(self):
        self.defines = {}
        self.log_file = None
        self.log_level = WARN
        self.log_categories = []


opts, args = parser.parse_args()

# Known bad tests, skipped unless specifically matched on the command line
known_bad = ["proton_tests.messenger.*"]

includes = []
excludes = ["*__*__"]
config = Config()
list_only = opts.list
for d in opts.defines:
    try:
        idx = d.index("=")
        name = d[:idx]
        value = d[idx + 1:]
        config.defines[name] = value
    except ValueError:
        config.defines[d] = None
config.log_file = opts.log_file
config.log_level = levels[opts.log_level.upper()]
config.log_categories = opts.log_categories
excludes.extend([v.strip() for v in opts.ignore])
for v in opts.ignore_file:
    f = open(v)
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            continue
        excludes.append(line)
    f.close()

for a in args:
    includes.append(a.strip())


def is_ignored(path):
    for p in excludes:
        if match(path, p):
            return True
    return False


def is_included(path):
    if is_ignored(path):
        return False
    for p in includes:
        if match(path, p):
            return True
    for p in known_bad:
        if match(path, p):
            return False
    return not includes           # If includes is empty, include everything


def is_smart():
    return sys.stdout.isatty() and os.environ.get("TERM", "dumb") != "dumb"


try:
    import fcntl
    import termios

    def width():
        if is_smart():
            s = struct.pack("HHHH", 0, 0, 0, 0)
            fd_stdout = sys.stdout.fileno()
            x = fcntl.ioctl(fd_stdout, termios.TIOCGWINSZ, s)
            rows, cols, xpx, ypx = struct.unpack("HHHH", x)
            return cols
        else:
            try:
                return int(os.environ.get("COLUMNS", "80"))
            except ValueError:
                return 80

    WIDTH = width()

    def resize(sig, frm):
        global WIDTH
        WIDTH = width()

    import signal
    signal.signal(signal.SIGWINCH, resize)

except ImportError:
    WIDTH = 80


def vt100_attrs(*attrs):
    return "\x1B[%sm" % ";".join(map(str, attrs))


vt100_reset = vt100_attrs(0)

KEYWORDS = {"pass": (32,),
            "skip": (33,),
            "fail": (31,),
            "start": (34,),
            "total": (34,),
            "ignored": (33,),
            "selected": (34,),
            "elapsed": (34,),
            "average": (34,)}


def colorize_word(word, text=None):
    if text is None:
        text = word
    return colorize(text, *KEYWORDS.get(word, ()))


def colorize(text, *attrs):
    if attrs and (is_smart() or opts.always_colorize):
        return "%s%s%s" % (vt100_attrs(*attrs), text, vt100_reset)
    else:
        return text


def indent(text):
    lines = text.split("\n")
    return "  %s" % "\n  ".join(lines)

# Write a 'minimal' Junit xml style report file suitable for use by CI tools such as Jenkins.


class JunitXmlStyleReporter:

    def __init__(self, file):
        self.f = open(file, "w")

    def begin(self):
        self.f.write('<?xml version="1.0" encoding="UTF-8" ?>\n')
        self.f.write('<testsuite name="pythontests">\n')

    def report(self, name, result):
        parts = name.split(".")
        method = parts[-1]
        module = '.'.join(parts[0:-1])
        self.f.write('<testcase classname="%s" name="%s" time="%f">\n' % (module, method, result.time))
        if result.failed:
            escaped_type = html.escape(str(result.exception_type), quote=True)
            escaped_message = html.escape(str(result.exception_message), quote=True)
            self.f.write(f'<failure type="{escaped_type}" message="{escaped_message}">\n')
            self.f.write('<![CDATA[\n')
            self.f.write(result.formatted_exception_trace)
            self.f.write(']]>\n')
            self.f.write('</failure>\n')
        if result.skipped:
            self.f.write('<skipped/>\n')
        self.f.write('</testcase>\n')

    def end(self):
        self.f.write('</testsuite>\n')
        self.f.close()


class Interceptor:

    def __init__(self):
        self.newline = False
        self.indent = False
        self.passthrough = True
        self.dirty = False
        self.last = None

    def begin(self):
        self.newline = True
        self.indent = True
        self.passthrough = False
        self.dirty = False
        self.last = None

    def reset(self):
        self.newline = False
        self.indent = False
        self.passthrough = True


class StreamWrapper:

    def __init__(self, interceptor, stream, prefix="  "):
        self.interceptor = interceptor
        self.stream = stream
        self.prefix = prefix

    def fileno(self):
        return self.stream.fileno()

    def isatty(self):
        return self.stream.isatty()

    def write(self, s):
        if self.interceptor.passthrough:
            self.stream.write(s)
            return

        if s:
            self.interceptor.dirty = True

        if self.interceptor.newline:
            self.interceptor.newline = False
            self.stream.write(" %s\n" % colorize_word("start"))
            self.interceptor.indent = True
        if self.interceptor.indent:
            self.stream.write(self.prefix)
        if s.endswith("\n"):
            s = s.replace("\n", "\n%s" % self.prefix)[:-2]
            self.interceptor.indent = True
        else:
            s = s.replace("\n", "\n%s" % self.prefix)
            self.interceptor.indent = False
        self.stream.write(s)

        if s:
            self.interceptor.last = s[-1]

    def flush(self):
        self.stream.flush()


interceptor = Interceptor()

out_wrp = StreamWrapper(interceptor, sys.stdout)
err_wrp = StreamWrapper(interceptor, sys.stderr)

out = sys.stdout
err = sys.stderr
sys.stdout = out_wrp
sys.stderr = err_wrp


class PatternFilter(Filter):

    def __init__(self, *patterns):
        Filter.__init__(self, patterns)
        self.patterns = patterns

    def filter(self, record):
        if not self.patterns:
            return True
        for p in self.patterns:
            if match(record.name, p):
                return True
        return False


root = getLogger()
handler = StreamHandler(sys.stdout)
filter = PatternFilter(*config.log_categories)
handler.addFilter(filter)
handler.setFormatter(Formatter("%(asctime)s %(levelname)s %(message)s"))
root.addHandler(handler)
root.setLevel(WARN)

log = getLogger("proton.test")

PASS = "pass"
SKIP = "skip"
FAIL = "fail"


class Runner:

    def __init__(self):
        self.exception = None
        self.exception_phase_name = None
        self.skip = False

    def passed(self):
        return not self.exception

    def skipped(self):
        return self.skip

    def failed(self):
        return self.exception and not self.skip

    def halt(self):
        """determines if the overall test execution should be allowed to continue to the next phase"""
        return self.exception or self.skip

    def run(self, phase_name, phase):
        """invokes a test-phase method (which can be the test method itself or a setUp/tearDown
           method).  If the method raises an exception the exception is examined to see if the
           exception should be classified as a 'skipped' test"""
        # we don't try to catch exceptions for jython because currently a
        # jython bug will prevent the java portion of the stack being
        # stored with the exception info in the sys module
        if opts.bare:
            phase()
        else:
            try:
                phase()
            except KeyboardInterrupt:
                raise
            except BaseException:
                self.exception_phase_name = phase_name
                self.exception = sys.exc_info()
                exception_type = self.exception[0]
                self.skip = getattr(exception_type, "skipped", False) or issubclass(exception_type, SkipTest)

    def status(self):
        if self.passed():
            return PASS
        elif self.skipped():
            return SKIP
        elif self.failed():
            return FAIL
        else:
            return None

    def get_formatted_exception_trace(self):
        if self.exception:
            if self.skip:
                # format skipped tests without a traceback
                output = indent("".join(traceback.format_exception_only(*self.exception[:2]))).rstrip()
            else:
                output = "Error during %s:" % self.exception_phase_name
                lines = traceback.format_exception(*self.exception)
                val = self.exception[1]
                reflect = False
                if val and hasattr(val, "getStackTrace"):
                    jlines = []
                    for frame in val.getStackTrace():
                        if reflect and frame.getClassName().startswith("org.python."):
                            # this is a heuristic to eliminate the jython portion of
                            # the java stack trace
                            if not opts.javatrace:
                                break
                        jlines.append("  %s\n" % frame.toString())
                        if frame.getClassName().startswith("java.lang.reflect"):
                            reflect = True
                        else:
                            reflect = False
                    lines.extend(jlines)
                output += indent("".join(lines)).rstrip()
            return output

    def get_exception_type(self):
        if self.exception:
            return self.exception[0]
        else:
            return None

    def get_exception_message(self):
        if self.exception:
            return self.exception[1]
        else:
            return None


ST_WIDTH = 8


def run_test(name, test, config):
    patterns = filter.patterns
    level = root.level
    filter.patterns = config.log_categories
    root.setLevel(config.log_level)

    parts = name.split(".")
    line = None
    output = ""
    for part in parts:
        if line:
            if len(line) + len(part) >= (WIDTH - ST_WIDTH - 1):
                output += "%s. \\\n" % line
                line = "    %s" % part
            else:
                line = "%s.%s" % (line, part)
        else:
            line = part

    if line:
        output += "%s %s" % (line, (((WIDTH - ST_WIDTH) - len(line)) * "."))
    sys.stdout.write(output)
    sys.stdout.flush()
    interceptor.begin()
    start = time.time()
    try:
        runner = test()
    finally:
        interceptor.reset()
    end = time.time()
    if interceptor.dirty:
        if interceptor.last != "\n":
            sys.stdout.write("\n")
        sys.stdout.write(output)
    print(" %s" % colorize_word(runner.status()))
    if runner.failed() or runner.skipped():
        print(runner.get_formatted_exception_trace())
    root.setLevel(level)
    filter.patterns = patterns
    return TestResult(end - start,
                      runner.passed(),
                      runner.skipped(),
                      runner.failed(),
                      runner.get_exception_type(),
                      runner.get_exception_message(),
                      runner.get_formatted_exception_trace())


class TestResult:

    def __init__(self, time, passed, skipped, failed, exception_type, exception_message, formatted_exception_trace):
        self.time = time
        self.passed = passed
        self.skipped = skipped
        self.failed = failed
        self.exception_type = exception_type
        self.exception_message = exception_message
        self.formatted_exception_trace = formatted_exception_trace


class FunctionTest:

    def __init__(self, test):
        self.test = test

    def name(self):
        return "%s.%s" % (self.test.__module__, self.test.__name__)

    def run(self):
        return run_test(self.name(), self._run, config)

    def _run(self):
        runner = Runner()
        runner.run("test", lambda: self.test(config))
        return runner

    def __repr__(self):
        return "FunctionTest(%r)" % self.test


class MethodTest:

    def __init__(self, cls, method):
        self.cls = cls
        self.method = method

    def name(self):
        return "%s.%s.%s" % (self.cls.__module__, self.cls.__name__, self.method)

    def run(self):
        return run_test(self.name(), self._run, config)

    def _run(self):
        runner = Runner()
        inst = self.cls(self.method)
        test = getattr(inst, self.method)

        if hasattr(inst, "configure"):
            runner.run("configure", lambda: inst.configure(config))
            if runner.halt():
                return runner
        if hasattr(inst, "setUp"):
            runner.run("setup", inst.setUp)
            if runner.halt():
                return runner

        runner.run("test", test)

        if hasattr(inst, "tearDown"):
            runner.run("teardown", inst.tearDown)

        return runner

    def __repr__(self):
        return "MethodTest(%r, %r)" % (self.cls, self.method)


class PatternMatcher:

    def __init__(self, *patterns):
        self.patterns = patterns

    def matches(self, name):
        for p in self.patterns:
            if match(name, p):
                return True
        return False


class FunctionScanner(PatternMatcher):

    def inspect(self, obj):
        return isinstance(obj, types.FunctionType) and self.matches(obj.__name__)

    def descend(self, func):
        return
        yield

    def extract(self, func):
        yield FunctionTest(func)


class ClassScanner(PatternMatcher):

    def inspect(self, obj):
        return isinstance(obj, type) and self.matches(obj.__name__)

    def descend(self, cls):
        return
        yield

    def extract(self, cls):
        for name in sorted(dir(cls)):
            obj = getattr(cls, name)
            if hasattr(obj, '__call__') and name.startswith("test"):
                yield MethodTest(cls, name)


class ModuleScanner:

    def inspect(self, obj):
        return isinstance(obj, types.ModuleType)

    def descend(self, obj):
        for name in sorted(dir(obj)):
            yield getattr(obj, name)

    def extract(self, obj):
        return
        yield


class Harness:

    def __init__(self):
        self.scanners = [
            ModuleScanner(),
            ClassScanner("*Test", "*Tests", "*TestCase"),
            FunctionScanner("test_*")
        ]
        self.tests = []
        self.scanned = []

    def scan(self, *roots):
        objects = list(roots)

        while objects:
            obj = objects.pop(0)
            for s in self.scanners:
                if s.inspect(obj):
                    self.tests.extend(s.extract(obj))
                    for child in s.descend(obj):
                        if not (child in self.scanned or child in objects):
                            objects.append(child)
            self.scanned.append(obj)


modules = opts.modules
if not modules:
    modules.extend(["proton_tests"])
h = Harness()
for name in modules:
    m = __import__(name, None, None, ["dummy"])
    h.scan(m)

filtered = [t for t in h.tests if is_included(t.name())]
ignored = [t for t in h.tests if is_ignored(t.name())]
total = len(filtered) + len(ignored)

if opts.xml and not list_only:
    xmlr = JunitXmlStyleReporter(opts.xml)
    xmlr.begin()
else:
    xmlr = None


def runthrough():
    passed = 0
    failed = 0
    skipped = 0
    start = time.time()
    for t in filtered:
        if list_only:
            print(t.name())
        else:
            st = t.run()
            if xmlr:
                xmlr.report(t.name(), st)
            if st.passed:
                passed += 1
            elif st.skipped:
                skipped += 1
            elif st.failed:
                failed += 1
                if opts.hoe:
                    break
    end = time.time()

    run = passed + failed

    if not list_only:
        if passed:
            _pass = "pass"
        else:
            _pass = "fail"
        if failed:
            outcome = "fail"
        else:
            outcome = "pass"
        if ignored:
            ign = "ignored"
        else:
            ign = "pass"
        if skipped:
            skip = "skip"
        else:
            skip = "pass"
        sys.stdout.write(colorize("Totals: ", 1))
        totals = [colorize_word("total", "%s tests" % total),
                  colorize_word(_pass, "%s passed" % passed),
                  colorize_word(skip, "%s skipped" % skipped),
                  colorize_word(ign, "%s ignored" % len(ignored)),
                  colorize_word(outcome, "%s failed" % failed)]
        sys.stdout.write(", ".join(totals))
        if opts.hoe and failed > 0:
            print(" -- (halted after %s)" % run)
        else:
            print("")
        if opts.time and run > 0:
            sys.stdout.write(colorize("Timing:", 1))
            timing = [colorize_word("elapsed", "%.2fs elapsed" % (end - start)),
                      colorize_word("average", "%.2fs average" % ((end - start) / run))]
            print(", ".join(timing))

    if xmlr:
        xmlr.end()

    return failed


def main():
    limit = opts.count
    count = 0
    failures = False
    while limit == 0 or count < limit:
        count += 1
        if runthrough():
            failures = True
            if count > 1:
                print(" -- (failures after %s runthroughs)" % count)
        else:
            continue

    if failures:
        return 1
    else:
        return 0
