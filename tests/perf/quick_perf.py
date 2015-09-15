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

# For use with CMake to run simple performance tests in Proton.
# Assumes that rector-recv and reactor-send can be found in PATH.
# CMkake's choice of python executable may be passed via PYTHON_EXE environment var.
# Add any OS specific monitor helpers in PN_QPERF_MON: i.e.
#    PN_QPERF_MON="time taskset 0x2" make quick_perf_c


import os, sys, socket, time
from example_test import background, verify, wait_addr, execute, pick_addr, cmdline
from subprocess import Popen, PIPE, STDOUT


NULL = open(os.devnull, 'w')

connaddr = pick_addr()
linkaddr = connaddr + "/perf_test"

if 'PYTHON_EXE' in os.environ:
    python_exe = os.environ['PYTHON_EXE']
else:
    python_exe = 'python'

if 'PN_QPERF_MON' in os.environ:
    monitor_cmd = os.environ['PN_QPERF_MON'].split()
else:
    monitor_cmd = []



mcount = 5000000
if 'PYTHON' in sys.argv:
    mcount /= 10

perf_targets = {'C' : ['reactor-send', "-a", linkaddr, "-c", str(mcount), "-R"],
                'CPP' : ['reactor_send_cpp', "-a", linkaddr, "-c", str(mcount), "-R", "1"],
                'PYTHON' : [python_exe, 'reactor-send.py', "-a", linkaddr, "-c", str(mcount), "-R"] }
try:
    perf_target = monitor_cmd + perf_targets[sys.argv[1]]
except:
    print "Usage: python quick_perf [C|CPP|PYTHON]"
    raise


# Use Proton-C reactor-recv as a relatively fast loopback "broker" for these tests
server = cmdline("reactor-recv", "-a", linkaddr, "-c", str(mcount), "-R")
try:
    recv = Popen(server, stdout=NULL, stderr=sys.stderr)
    wait_addr(connaddr)
    start = time.time()
    execute(*perf_target)
    end = time.time()
    verify(recv)
except Exception as e:
    if recv: recv.kill()
    raise Exception("Error running %s: %s", server, e)


secs = end - start
print("%d loopback messages in %.1f secs" % (mcount * 2, secs) )
print("%.0f msgs/sec" % (mcount * 2 / secs) )
