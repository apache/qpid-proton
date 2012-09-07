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

class SaslTest(Test):

  def testPipelined(self):
    cli = pn_transport()
    cli_auth = pn_sasl(cli)
    pn_sasl_mechanisms(cli_auth, "ANONYMOUS")
    pn_sasl_client(cli_auth)

    assert pn_sasl_outcome(cli_auth) == PN_SASL_NONE

    srv = pn_transport()
    srv_auth = pn_sasl(srv)
    pn_sasl_mechanisms(srv_auth, "ANONYMOUS")
    pn_sasl_server(srv_auth)
    pn_sasl_done(srv_auth, PN_SASL_OK)

    cli_code, cli_out = pn_output(cli, 1024)
    srv_code, srv_out = pn_output(srv, 1024)

    assert cli_code > 0, cli_code
    assert srv_code > 0, srv_code

    n = pn_input(srv, cli_out)
    assert n == len(cli_out), "(%s) %s" % (n, pn_error_text(pn_transport_error(srv)))

    assert pn_sasl_outcome(cli_auth) == PN_SASL_NONE

    n = pn_input(cli, srv_out)
    assert n == len(srv_out), n

    assert pn_sasl_outcome(cli_auth) == PN_SASL_OK

    pn_transport_free(cli)
    pn_transport_free(srv)
