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

"""
Utilities to help Proton support both python2 and python3.
"""

import sys
import types
IS_PY2 = sys.version_info[0] == 2
IS_PY3 = sys.version_info[0] == 3

if IS_PY3:
    INT_TYPES = (int,)
    TEXT_TYPES = (str,)
    STRING_TYPES = (str,)
    BINARY_TYPES = (bytes,)
    CLASS_TYPES = (type,)

    def raise_(t, v=None, tb=None):
        """Mimic the old 2.x raise behavior:
        Raise an exception of type t with value v using optional traceback tb
        """
        if v is None:
            v = t()
        if tb is None:
            raise v
        else:
            raise v.with_traceback(tb)

    def iteritems(d):
        return iter(d.items())

    def unichar(i):
        return chr(i)

    def str2bin(s, encoding='latin-1'):
        """Convert str to binary type"""
        return s.encode(encoding)

    def str2unicode(s):
        return s

else:
    INT_TYPES = (int, long)
    TEXT_TYPES = (unicode,)
    # includes both unicode and non-unicode strings:
    STRING_TYPES = (basestring,)
    BINARY_TYPES = (str,)
    CLASS_TYPES = (type, types.ClassType)

    # the raise syntax will cause a parse error in Py3, so 'sneak' in a
    # definition that won't cause the parser to barf
    exec("""def raise_(t, v=None, tb=None):
    raise t, v, tb
""")

    def iteritems(d, **kw):
        return d.iteritems()

    def unichar(i):
        return unichr(i)

    def str2bin(s, encoding='latin-1'):
        return s

    def str2unicode(s):
        return unicode(s, "unicode_escape")
