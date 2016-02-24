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
from org.apache.qpid.proton import Proton

# from proton/driver.h

def pn_driver():
  return Proton.driver()

def pn_driver_wait(drv, t):
  drv.doWait(t)

def pn_driver_listener(drv):
  return drv.listener()

def pn_driver_connector(drv):
  return drv.connector()

def pn_listener(drv, host, port, ctx):
  return drv.createListener(host, int(port), ctx)

def pn_listener_context(l):
  return l.getContext()

def pn_listener_set_context(l, v):
  l.setContext(v)

def pn_listener_accept(l):
  return l.accept()

def pn_connector(drv, host, port, ctx):
  return drv.createConnector(host, int(port), ctx)

def pn_connector_context(c):
  return c.getContext()

def pn_connector_set_context(c, v):
  c.setContext(v)

def pn_connector_set_connection(c, conn):
  c.setConnection(conn.impl)

def pn_connector_connection(c):
  return wrap(c.getConnection(), pn_connection_wrapper)

def pn_connector_transport(c):
  return wrap(c.getTransport(), pn_transport_wrapper)

def pn_connector_process(c):
  return c.process()

def pn_connector_closed(c):
  return c.isClosed()
