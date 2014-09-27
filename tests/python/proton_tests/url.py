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


import common
from proton import Url

class UrlTest(common.Test):
    def assertEqual(self, a, b):
        assert a == b, "%s != %s" % (a, b)

    def assertNotEqual(self, a, b):
        assert a != b, "%s == %s" % (a, b)

    def assertUrl(self, u, scheme, username, password, host, port, path):
        self.assertEqual((u.scheme, u.username, u.password, u.host, u.port, u.path),
                         (scheme, username, password, host, port, path))

    def testUrl(self):
        url = Url('amqp://me:secret@myhost:1234/foobar')
        self.assertEqual(str(url), "amqp://me:secret@myhost:1234/foobar")
        self.assertUrl(url, 'amqp', 'me', 'secret', 'myhost', 1234, 'foobar')
        self.assertEqual(str(url), "amqp://me:secret@myhost:1234/foobar")

    def testDefaults(self):
        # Check that we allow None for scheme, port
        url = Url(username='me', password='secret', host='myhost', path='foobar')
        self.assertEqual(str(url), "me:secret@myhost/foobar")
        self.assertUrl(url, None, 'me', 'secret', 'myhost', None, 'foobar')

        self.assertEqual(str(Url("amqp://me:secret@myhost/foobar").defaults()),
                         "amqp://me:secret@myhost:amqp/foobar")

        # Empty string vs. None for path
        self.assertEqual(Url("myhost/").path, "")
        assert Url("myhost").path is None

    def assertPort(self, port, portint, portstr):
        self.assertEqual((port, port), (portint, portstr))
        self.assertEqual((int(port), str(port)), (portint, portstr))

    def testPort(self):
        self.assertPort(Url.Port('amqp'), 5672, 'amqp')
        self.assertPort(Url.Port(5672), 5672, '5672')
        self.assertPort(Url.Port(5671), 5671, '5671')
        self.assertEqual(Url.Port(5671)+1, 5672) # Treat as int
        self.assertEqual(str(Url.Port(5672)), '5672')

        self.assertPort(Url.Port(Url.Port('amqp')), 5672, 'amqp')
        self.assertPort(Url.Port(Url.Port(5672)), 5672, '5672')

        try:
            Url.Port('xxx')
            assert False, "Expected ValueError"
        except ValueError: pass

        self.assertEqual(str(Url("host:amqp")), "host:amqp")
        self.assertEqual(Url("host:amqp").port, 5672)

    def testArgs(self):
        u = Url("amqp://u:p@host:amqp/path", scheme='foo', host='bar', port=1234, path='garden')
        self.assertUrl(u, 'foo', 'u', 'p', 'bar', 1234, 'garden')
        u = Url()
        self.assertUrl(u, None, None, None, None, None, None)

    def assertRaises(self, exception, function, *args, **kwargs):
        try:
            function(*args, **kwargs)
            assert False, "Expected exception %s" % exception.__name__
        except exception: pass

    def testMissing(self):
        self.assertUrl(Url(), None, None, None, None, None, None)
        self.assertUrl(Url('amqp://'), 'amqp', None, None, None, None, None)
        self.assertUrl(Url('username@'), None, 'username', None, None, None, None)
        self.assertUrl(Url(':pass@'), None, '', 'pass', None, None, None)
        self.assertUrl(Url('host'), None, None, None, 'host', None, None)
        self.assertUrl(Url(':1234'), None, None, None, None, 1234, None)
        self.assertUrl(Url('/path'), None, None, None, None, None, 'path')

        for s in ['amqp://', 'username@', ':pass@', ':1234', '/path']:
            self.assertEqual(s, str(Url(s)))

        for s, full in [
                ('amqp://', 'amqp://0.0.0.0:amqp'),
                ('username@', 'amqp://username@0.0.0.0:amqp'),
                (':pass@', 'amqp://:pass@0.0.0.0:amqp'),
                (':1234', 'amqp://0.0.0.0:1234'),
                ('/path', 'amqp://0.0.0.0:amqp/path')]:
            self.assertEqual(str(Url(s).defaults()), full)

    def testAmqps(self):
        """Some old platforms don't recognize the amqps service name, this test is a no-op
        if that is the case otherwise verify we treat amqps correctly."""
        try:
            Url.Port('amqps')
        except ValueError:
            print "skipping: service 'amqps' not recognized on this platform"
            return
        # Scheme defaults
        self.assertEqual(str(Url("me:secret@myhost/foobar").defaults()),
                         "amqp://me:secret@myhost:amqp/foobar")
        # Correct port for amqps vs. amqps
        self.assertEqual(str(Url("amqps://me:secret@myhost/foobar").defaults()),
                         "amqps://me:secret@myhost:amqps/foobar")

        self.assertPort(Url.Port('amqps'), 5671, 'amqps')
        self.assertEqual(str(Url("host:amqps")), "host:amqps")
        self.assertEqual(Url("host:amqps").port, 5671)
