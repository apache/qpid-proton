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
    cli = pn_sasl()
    pn_sasl_mechanisms(cli, "ANONYMOUS")
    pn_sasl_client(cli)

    srv = pn_sasl()
    pn_sasl_mechanisms(srv, "ANONYMOUS")
    pn_sasl_server(srv)
    pn_sasl_done(srv, PN_SASL_OK)

    cli_code, cli_out = pn_sasl_output(cli, 1024)
    srv_code, srv_out = pn_sasl_output(srv, 1024)

    assert cli_code > 0, cli_code
    assert srv_code > 0, srv_code

    dummy_header = "AMQPxxxx"

    srv_out += dummy_header
    cli_out += dummy_header

    n = pn_sasl_input(srv, cli_out)
    assert n > 0, n
    cli_out = cli_out[n:]
    assert cli_out == dummy_header
    n = pn_sasl_input(srv, cli_out)
    assert n == PN_EOS, n

    n = pn_sasl_input(cli, srv_out)
    assert n > 0, n
    srv_out = srv_out[n:]
    assert srv_out == dummy_header
    n = pn_sasl_input(cli, srv_out)
    assert n == PN_EOS, n
