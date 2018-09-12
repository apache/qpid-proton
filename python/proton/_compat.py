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

# bridge between py2 Queue renamed as py3 queue
try:
    import Queue as queue
except ImportError:
    import queue # type: ignore

try:
    from urlparse import urlparse, urlunparse
    from urllib import quote, unquote # type: ignore
except ImportError:
    from urllib.parse import urlparse, urlunparse, quote, unquote

PY3 = sys.version_info[0] == 3

if PY3:
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


    def iteritems(d, **kw):
        return iter(d.items(**kw))


    unichr = chr
else:
    # the raise syntax will cause a parse error in Py3, so 'sneak' in a
    # definition that won't cause the parser to barf
    exec ("""def raise_(t, v=None, tb=None):
    raise t, v, tb
""")


    def iteritems(d, **kw):
        return d.iteritems(**kw)


    unichr = unichr
