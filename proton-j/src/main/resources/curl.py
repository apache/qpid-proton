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

from org.apache.qpid.proton.messenger.impl import Address

def pn_url():
    return Address()

def pn_url_parse(urlstr):
    return Address(urlstr)

def pn_url_free(url): pass

def pn_url_clear(url):
    url.clear();

def pn_url_str(url): return url.toString()

def pn_url_get_scheme(url): return url.getScheme()
def pn_url_get_username(url): return url.getUser()
def pn_url_get_password(url): return url.getPass()
def pn_url_get_host(url): return url.getHost() or None
def pn_url_get_port(url): return url.getPort()
def pn_url_get_path(url): return url.getName()

def pn_url_set_scheme(url, value): url.setScheme(value)
def pn_url_set_username(url, value): url.setUser(value)
def pn_url_set_password(url, value): url.setPass(value)
def pn_url_set_host(url, value): url.setHost(value)
def pn_url_set_port(url, value): url.setPort(value)
def pn_url_set_path(url, value): url.setName(value)
