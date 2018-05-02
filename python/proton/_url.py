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

from __future__ import absolute_import

import socket

from cproton import pn_url, pn_url_free, pn_url_parse, pn_url_str, pn_url_get_port, pn_url_get_scheme, \
    pn_url_get_host, pn_url_get_username, pn_url_get_password, pn_url_get_path, pn_url_set_scheme, pn_url_set_host, \
    pn_url_set_username, pn_url_set_password, pn_url_set_port, pn_url_set_path

from ._common import unicode2utf8


class Url(object):
    """
    Simple URL parser/constructor, handles URLs of the form:

    <scheme>://<user>:<password>@<host>:<port>/<path>

    All components can be None if not specified in the URL string.

    The port can be specified as a service name, e.g. 'amqp' in the
    URL string but Url.port always gives the integer value.

    Warning: The placement of user and password in URLs is not
    recommended.  It can result in credentials leaking out in program
    logs.  Use connection configuration attributes instead.

    @ivar scheme: Url scheme e.g. 'amqp' or 'amqps'
    @ivar user: Username
    @ivar password: Password
    @ivar host: Host name, ipv6 literal or ipv4 dotted quad.
    @ivar port: Integer port.
    @ivar host_port: Returns host:port
    """

    AMQPS = "amqps"
    AMQP = "amqp"

    class Port(int):
        """An integer port number that can be constructed from a service name string"""

        def __new__(cls, value):
            """@param value: integer port number or string service name."""
            port = super(Url.Port, cls).__new__(cls, cls._port_int(value))
            setattr(port, 'name', str(value))
            return port

        def __eq__(self, x):
            return str(self) == x or int(self) == x

        def __ne__(self, x):
            return not self == x

        def __str__(self):
            return str(self.name)

        @staticmethod
        def _port_int(value):
            """Convert service, an integer or a service name, into an integer port number."""
            try:
                return int(value)
            except ValueError:
                try:
                    return socket.getservbyname(value)
                except socket.error:
                    # Not every system has amqp/amqps defined as a service
                    if value == Url.AMQPS:
                        return 5671
                    elif value == Url.AMQP:
                        return 5672
                    else:
                        raise ValueError("Not a valid port number or service name: '%s'" % value)

    def __init__(self, url=None, defaults=True, **kwargs):
        """
        @param url: URL string to parse.
        @param defaults: If true, fill in missing default values in the URL.
            If false, you can fill them in later by calling self.defaults()
        @param kwargs: scheme, user, password, host, port, path.
          If specified, replaces corresponding part in url string.
        """
        if url:
            self._url = pn_url_parse(unicode2utf8(str(url)))
            if not self._url: raise ValueError("Invalid URL '%s'" % url)
        else:
            self._url = pn_url()
        for k in kwargs:  # Let kwargs override values parsed from url
            getattr(self, k)  # Check for invalid kwargs
            setattr(self, k, kwargs[k])
        if defaults: self.defaults()

    class PartDescriptor(object):
        def __init__(self, part):
            self.getter = globals()["pn_url_get_%s" % part]
            self.setter = globals()["pn_url_set_%s" % part]

        def __get__(self, obj, type=None): return self.getter(obj._url)

        def __set__(self, obj, value): return self.setter(obj._url, str(value))

    scheme = PartDescriptor('scheme')
    username = PartDescriptor('username')
    password = PartDescriptor('password')
    host = PartDescriptor('host')
    path = PartDescriptor('path')

    def _get_port(self):
        portstr = pn_url_get_port(self._url)
        return portstr and Url.Port(portstr)

    def _set_port(self, value):
        if value is None:
            pn_url_set_port(self._url, None)
        else:
            pn_url_set_port(self._url, str(Url.Port(value)))

    port = property(_get_port, _set_port)

    def __str__(self):
        return pn_url_str(self._url)

    def __repr__(self):
        return "Url(%s://%s/%s)" % (self.scheme, self.host, self.path)

    def __eq__(self, x):
        return str(self) == str(x)

    def __ne__(self, x):
        return not self == x

    def __del__(self):
        pn_url_free(self._url)
        del self._url

    def defaults(self):
        """
        Fill in missing values (scheme, host or port) with defaults
        @return: self
        """
        self.scheme = self.scheme or self.AMQP
        self.host = self.host or '0.0.0.0'
        self.port = self.port or self.Port(self.scheme)
        return self
