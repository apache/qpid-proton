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
This module provides document parsing and transformation utilities for XML.
"""

from __future__ import absolute_import

import os
import sys
import xml.sax
import types
from xml.sax.handler import ErrorHandler
from xml.sax.xmlreader import InputSource

try:
    from io import StringIO
except ImportError:
    from cStringIO import StringIO

if sys.version_info[0] == 2:
    import types
    CLASS_TYPES = (type, types.ClassType)
else:
    CLASS_TYPES = (type,)

from . import dom
from . import transforms
from . import parsers


def transform(node, *args):
    result = node
    for t in args:
        if isinstance(t, CLASS_TYPES):
            t = t()
        result = result.dispatch(t)
    return result


class Resolver:

    def __init__(self, path):
        self.path = path

    def resolveEntity(self, publicId, systemId):
        for p in self.path:
            fname = os.path.join(p, systemId)
            if os.path.exists(fname):
                source = InputSource(systemId)
                source.setByteStream(open(fname))
                return source
        return InputSource(systemId)


def xml_parse(filename, path=()):
    h = parsers.XMLParser()
    p = xml.sax.make_parser()
    p.setContentHandler(h)
    p.setErrorHandler(ErrorHandler())
    p.setEntityResolver(Resolver(path))
    p.parse(filename)
    return h.parser.tree


def sexp(node):
    s = transforms.Sexp()
    node.dispatch(s)
    return s.out
