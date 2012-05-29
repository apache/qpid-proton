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

from org.apache.qpid.proton.engine import *
from jarray import zeros

PN_EOS = Transport.END_OF_STREAM

def pn_connection():
  return impl.ConnectionImpl()

def pn_connection_open(c):
  return c.open()

def pn_session(c):
  return c.session()

def pn_session_open(s):
  return s.open()

def pn_transport(c):
  return c.transport()

def pn_output(t, size):
  output = zeros(size, "b")
  n = t.output(output, 0, size)
  result = ""
  if n > 0:
    result = output.tostring()[:n]
  return [n, result]

def pn_input(t, inp):
  return t.input(inp, 0, len(inp))
