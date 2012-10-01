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

import os, common
from proton import *

class Test(common.Test):
  pass

class SaslTest(Test):

  def testPipelined(self):
    cli = Transport()
    cli_auth = SASL(cli)
    cli_auth.mechanisms("ANONYMOUS")
    cli_auth.client()

    assert cli_auth.outcome is None

    srv = Transport()
    srv_auth = SASL(srv)
    srv_auth.mechanisms("ANONYMOUS")
    srv_auth.server()
    srv_auth.done(SASL.OK)

    cli_out = cli.output(1024)
    srv_out = srv.output(1024)

    n = srv.input(cli_out)
    assert n == len(cli_out), (n, cli_out)

    assert cli_auth.outcome is None

    n = cli.input(srv_out)
    assert n == len(srv_out), n

    assert cli_auth.outcome == SASL.OK
