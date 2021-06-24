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


#
# Hacks to provide Python2 <---> Python3 compatibility
#
# The results are
# |       |long|unicode|
# |python2|long|unicode|
# |python3| int|    str|
try:
    long()
except NameError:
    long = int
try:
    unicode()
except NameError:
    unicode = str

from _proton_core import ffi


def isinteger(value):
    return isinstance(value, (int, long))


def isstring(value):
    return isinstance(value, (str, unicode))


class Constant(object):

    def __init__(self, name):
        self.name = name

    def __repr__(self):
        return self.name


def secs2millis(secs):
    return long(secs * 1000)


def millis2secs(millis):
    return float(millis) / 1000.0


def unicode2utf8(string):
    """Some Proton APIs expect a null terminated string. Convert python text
    types to UTF8 to avoid zero bytes introduced by other multi-byte encodings.
    This method will throw if the string cannot be converted.
    """
    if string is None or string == ffi.NULL:
        return None
    elif isinstance(string, str):
        return string
    # Anything else illegal - specifically python3 bytes
    raise TypeError(f"Unrecognized string type: {string!r} ({type(string)})")


def utf82unicode(string):
    """Convert C strings returned from proton-c into python unicode"""
    if string is None or string == ffi.NULL:
        return None
    elif isinstance(string, str):
        return string
    elif isinstance(string, bytes):
        return string.decode('utf8')
    elif isinstance(string, ffi.CData):
        # ffi.string return bytes for cdata type <check this comment pleaseeeeeeeeeeeeeeeeeeee>
        return ffi.string(string).decode()

    raise TypeError(f"Unrecognized string type: {string!r} ({type(string)})", string)
