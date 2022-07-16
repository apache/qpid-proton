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

from typing import Optional, Union, Any

from _proton_core import ffi


def isinteger(value: Any) -> bool:
    return isinstance(value, int)


def isstring(value: Any) -> bool:
    return isinstance(value, str)


class Constant(object):

    def __init__(self, name: str) -> None:
        self.name = name

    def __repr__(self) -> str:
        return self.name


def secs2millis(secs: Union[float, int]) -> int:
    return int(secs * 1000)


def millis2secs(millis: int) -> float:
    return float(millis) / 1000.0


def unicode2utf8(string: Optional[str]) -> Optional[str]:
    """Some Proton APIs expect a null terminated string. Convert python text
    types to UTF8 to avoid zero bytes introduced by other multi-byte encodings.
    This method will throw if the string cannot be converted.
    """
    assert string != ffi.NULL
    if string is None or string == ffi.NULL:  # todo what's the directon of the conversion here?
        return ffi.NULL
    elif isinstance(string, str):
        # The swig binding converts py3 str -> utf8 char* and back automatically
        # TODO but cffi won't, and there is additional problem about the value lifetime which I am unsure about
        return string.encode()
    elif isinstance(string, ffi.CData):
        return ffi.string(string)
    # Anything else illegal - specifically python3 bytes
    raise TypeError("Unrecognized string type: %r (%s)" % (string, type(string)))


def utf82unicode(string: Optional[Union[str, bytes]]) -> Optional[str]:
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
