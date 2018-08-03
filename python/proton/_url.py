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

from ._compat import urlparse, urlunparse, quote, unquote


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
        if isinstance(url, Url):
            self.scheme = url.scheme
            self.username = url.username
            self.password = url.password
            self._host = url._host
            self._port = url._port
            self._path = url._path
            self._params = url._params
            self._query = url._query
            self._fragment = url._fragment
        elif url:
            if not url.startswith('//'):
                p = url.partition(':')
                if '/' in p[0] or not p[2].startswith('//'):
                    url = '//' + url
            u = urlparse(url)
            if not u: raise ValueError("Invalid URL '%s'" % url)
            self.scheme = None if not u.scheme else u.scheme
            self.username = u.username and unquote(u.username)
            self.password = u.password and unquote(u.password)
            (self._host, self._port) = self._parse_host_port(u.netloc)
            self._path = None if not u.path else u.path
            self._params = u.params
            self._query = u.query
            self._fragment = u.fragment
        else:
            self.scheme = None
            self.username = None
            self.password = None
            self._host = None
            self._port = None
            self._path = None
            self._params = None
            self._query = None
            self._fragment = None
        for k in kwargs:  # Let kwargs override values parsed from url
            getattr(self, k)  # Check for invalid kwargs
            setattr(self, k, kwargs[k])
        if defaults: self.defaults()

    @staticmethod
    def _parse_host_port(nl):
        hostport = nl.split('@')[-1]
        hostportsplit = hostport.split(']')
        beforebrace = hostportsplit[0]
        afterbrace = hostportsplit[-1]

        if len(hostportsplit)==1:
            beforebrace = ''
        else:
            beforebrace += ']'
        if ':' in afterbrace:
            afterbracesplit = afterbrace.split(':')
            port = afterbracesplit[1]
            host = (beforebrace+afterbracesplit[0]).lower()
            if not port:
                port = None
        else:
            host = (beforebrace+afterbrace).lower()
            port = None
        if not host:
            host = None
        return (host, port)

    @property
    def path(self):
        return self._path if not self._path or self._path[0] != '/' else self._path[1:]

    @path.setter
    def path(self, p):
        self._path = p if p[0] == '/' else '/' + p

    @staticmethod
    def _ipv6literal(s):
        return s.startswith('[') and s.endswith(']')

    @property
    def host(self):
        if self._host and self._ipv6literal(self._host):
            return self._host[1:-1]
        else:
            return self._host

    @host.setter
    def host(self, h):
        if ':' in h and not self._ipv6literal(h):
            self._host = '[' + h + ']'
        else:
            self._host = h

    @property
    def port(self):
        return self._port and Url.Port(self._port)

    @port.setter
    def port(self, p):
        self._port = p

    @property
    def _netloc(self):
        hostport = ''
        if self._host:
            hostport = self._host
        if self._port:
            hostport += ':'
            hostport += str(self._port)
        userpart = ''
        if self.username:
            userpart += quote(self.username)
        if self.password:
            userpart += ':'
            userpart += quote(self.password)
        if self.username or self.password:
            userpart += '@'
        return userpart + hostport

    def __str__(self):
        if self.scheme \
                and not self._netloc and not self._path \
                and not self._params and not self._query and not self._fragment:
            return self.scheme + '://'
        return urlunparse((self.scheme or '', self._netloc or '', self._path or '',
                           self._params or '', self._query or '', self._fragment or ''))

    def __repr__(self):
        return "Url('%s')" % self

    def __eq__(self, x):
        return str(self) == str(x)

    def __ne__(self, x):
        return not self == x

    def defaults(self):
        """
        Fill in missing values (scheme, host or port) with defaults
        @return: self
        """
        self.scheme = self.scheme or self.AMQP
        self._host = self._host or '0.0.0.0'
        self._port = self._port or self.Port(self.scheme)
        return self
