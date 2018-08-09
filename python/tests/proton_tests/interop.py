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

import os

from proton import *

from . import common


def find_test_interop_dir():
    """Find the common tests directory relative to this script"""
    from os.path import dirname, join, abspath, isdir
    f = dirname(dirname(dirname(dirname(abspath(__file__)))))
    f = join(f, "tests", "interop")
    if not isdir(f):
        raise Exception("Cannot find tests/interop directory from "+__file__)
    return f

test_interop_dir=find_test_interop_dir()

class InteropTest(common.Test):

    def setUp(self):
        self.data = Data()
        self.message = Message()

    def tearDown(self):
        self.data = None

    def get_data(self, name):
        filename = os.path.join(test_interop_dir, name+".amqp")
        f = open(filename,"rb")
        try: return f.read()
        finally: f.close()

    def decode_data(self, encoded):
        buffer = encoded
        while buffer:
            n = self.data.decode(buffer)
            buffer = buffer[n:]
        self.data.rewind()

    def decode_data_file(self, name):
        encoded = self.get_data(name)
        self.decode_data(encoded)
        encoded_size = self.data.encoded_size()
        # Re-encode and verify pre-computed and actual encoded size match.
        reencoded = self.data.encode()
        assert encoded_size == len(reencoded), "%d != %d" % (encoded_size, len(reencoded))
        # verify round trip bytes
        assert reencoded == encoded, "Value mismatch: %s != %s" % (reencoded, encoded)

    def decode_message_file(self, name):
        self.message.decode(self.get_data(name))
        body = self.message.body
        if str(type(body)) == "<type 'org.apache.qpid.proton.amqp.Binary'>":
            body = body.array.tostring()
        self.decode_data(body)

    def assert_next(self, type, value):
        next_type = self.data.next()
        assert next_type == type, "Type mismatch: %s != %s"%(
            Data.type_names[next_type], Data.type_names[type])
        next_value = self.data.get_object()
        assert next_value == value, "Value mismatch: %s != %s"%(next_value, value)

    def test_message(self):
        self.decode_message_file("message")
        self.assert_next(Data.STRING, "hello")
        assert self.data.next() is None

    def test_primitives(self):
        self.decode_data_file("primitives")
        self.assert_next(Data.BOOL, True)
        self.assert_next(Data.BOOL, False)
        self.assert_next(Data.UBYTE, 42)
        self.assert_next(Data.USHORT, 42)
        self.assert_next(Data.SHORT, -42)
        self.assert_next(Data.UINT, 12345)
        self.assert_next(Data.INT, -12345)
        self.assert_next(Data.ULONG, 12345)
        self.assert_next(Data.LONG, -12345)
        self.assert_next(Data.FLOAT, 0.125)
        self.assert_next(Data.DOUBLE, 0.125)
        assert self.data.next() is None

    def test_strings(self):
        self.decode_data_file("strings")
        self.assert_next(Data.BINARY, b"abc\0defg")
        self.assert_next(Data.STRING, "abcdefg")
        self.assert_next(Data.SYMBOL, "abcdefg")
        self.assert_next(Data.BINARY, b"")
        self.assert_next(Data.STRING, "")
        self.assert_next(Data.SYMBOL, "")
        assert self.data.next() is None

    def test_described(self):
        self.decode_data_file("described")
        self.assert_next(Data.DESCRIBED, Described("foo-descriptor", "foo-value"))
        self.data.exit()

        assert self.data.next() == Data.DESCRIBED
        self.data.enter()
        self.assert_next(Data.INT, 12)
        self.assert_next(Data.INT, 13)
        self.data.exit()

        assert self.data.next() is None

    def test_described_array(self):
        self.decode_data_file("described_array")
        self.assert_next(Data.ARRAY, Array("int-array", Data.INT, *range(0,10)))

    def test_arrays(self):
        self.decode_data_file("arrays")
        self.assert_next(Data.ARRAY, Array(UNDESCRIBED, Data.INT, *range(0,100)))
        self.assert_next(Data.ARRAY, Array(UNDESCRIBED, Data.STRING, *["a", "b", "c"]))
        self.assert_next(Data.ARRAY, Array(UNDESCRIBED, Data.INT))
        assert self.data.next() is None

    def test_lists(self):
        self.decode_data_file("lists")
        self.assert_next(Data.LIST, [32, "foo", True])
        self.assert_next(Data.LIST, [])
        assert self.data.next() is None

    def test_maps(self):
        self.decode_data_file("maps")
        self.assert_next(Data.MAP, {"one":1, "two":2, "three":3 })
        self.assert_next(Data.MAP, {1:"one", 2:"two", 3:"three"})
        self.assert_next(Data.MAP, {})
        assert self.data.next() is None
