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

import os, common, xproton
from xproton import *

class Test(common.Test):
  pass

class TransportTest(Test):

  def setup(self):
    self.transport = pn_transport()

  def teardown(self):
    pn_transport_free(self.transport)
    self.transport = None

  def testEOS(self):
    n = pn_input(self.transport, "")
    assert n == PN_ERR, pn_error_text(pn_transport_error(self.transport))

  def testPartial(self):
    n = pn_input(self.transport, "AMQ")
    assert n == 3, n
    n = pn_input(self.transport, "")
    assert n == PN_ERR, n

  def testGarbage(self):
    n = pn_input(self.transport, "GARBAGE_")
    assert n == PN_ERR, pn_error_text(pn_transport_error(self.transport))
    n = pn_input(self.transport, "")
    assert n == PN_ERR, n

  def testSmallGarbage(self):
    n = pn_input(self.transport, "XXX")
    assert n == PN_ERR, pn_error_text(pn_transport_error(self.transport))
    n = pn_input(self.transport, "")
    assert n == PN_ERR, n

  def testBigGarbage(self):
    n = pn_input(self.transport, "GARBAGE_XXX")
    assert n == PN_ERR, pn_error_text(pn_transport_error(self.transport))
    n = pn_input(self.transport, "")
    assert n == PN_ERR, n

  def testHeader(self):
    n = pn_input(self.transport, "AMQP\x00\x01\x00\x00")
    assert n == 8, n
    n = pn_input(self.transport, "")
    assert n == PN_ERR, n

  def testOutput(self):
    n, out = pn_output(self.transport, 1024)
    assert n == len(out)

  def testBindAfterOpen(self):
    conn = pn_connection()
    ssn = pn_session(conn)
    pn_connection_open(conn)
    pn_session_open(ssn)
    pn_connection_set_container(conn, "test-container")
    pn_connection_set_hostname(conn, "test-hostname")
    trn = pn_transport()
    pn_transport_bind(trn, conn)
    n, out = pn_output(trn, 1024)
    assert n == len(out), n
    assert "test-container" in out, repr(out)
    n = pn_input(self.transport, out)
    assert n > 0 and n < len(out)
    out = out[n:]

    n = pn_input(self.transport, out)
    assert n == 0

    c = pn_connection()
    assert pn_connection_remote_container(c) == None
    assert pn_connection_remote_hostname(c) == None
    pn_transport_bind(self.transport, c)
    assert pn_connection_remote_container(c) == "test-container"
    assert pn_connection_remote_hostname(c) == "test-hostname"
    assert pn_session_head(c, 0) == None
    n = pn_input(self.transport, out)
    assert n == len(out), (n, out)
    assert pn_session_head(c, 0) != None

    pn_transport_free(trn)
    pn_connection_free(conn)
    pn_connection_free(c)
