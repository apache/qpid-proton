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
The proton module defines a suite of APIs that implement the AMQP 1.0
protocol.

The proton APIs consist of the following classes:

 - :class:`Message`    -- A class for creating and/or accessing AMQP message content.
 - :class:`Data`       -- A class for creating and/or accessing arbitrary AMQP encoded data.
"""
from __future__ import absolute_import

import logging
import logging.config
import os

from cproton import PN_VERSION_MAJOR, PN_VERSION_MINOR, PN_VERSION_POINT

from ._condition import Condition
from ._data import UNDESCRIBED, Array, Data, Described, char, symbol, timestamp, ubyte, ushort, uint, ulong, \
    byte, short, int32, float32, decimal32, decimal64, decimal128, AnnotationDict, PropertyDict, SymbolList
from ._delivery import Delivery, Disposition
from ._endpoints import Endpoint, Connection, Session, Link, Receiver, Sender, Terminus
from ._events import Collector, Event, EventType, Handler
from ._exceptions import ProtonException, MessageException, DataException, TransportException, \
    SSLException, SSLUnavailable, ConnectionException, SessionException, LinkException, Timeout, Interrupt
from ._message import Message
from ._transport import Transport, SASL, SSL, SSLDomain, SSLSessionDetails
from ._url import Url

__all__ = [
    "API_LANGUAGE",
    "IMPLEMENTATION_LANGUAGE",
    "UNDESCRIBED",
    "AnnotationDict",
    "Array",
    "Collector",
    "Condition",
    "Connection",
    "ConnectionException",
    "Data",
    "DataException",
    "Delivery",
    "Disposition",
    "Described",
    "Endpoint",
    "Event",
    "EventType",
    "Handler",
    "Link",
    "LinkException",
    "Message",
    "MessageException",
    "PropertyDict",
    "ProtonException",
    "VERSION_MAJOR",
    "VERSION_MINOR",
    "Receiver",
    "SASL",
    "Sender",
    "Session",
    "SessionException",
    "SSL",
    "SSLDomain",
    "SSLSessionDetails",
    "SSLUnavailable",
    "SSLException",
    "SymbolList",
    "Terminus",
    "Timeout",
    "Interrupt",
    "Transport",
    "TransportException",
    "Url",
    "char",
    "symbol",
    "timestamp",
    "ulong",
    "byte",
    "short",
    "int32",
    "ubyte",
    "ushort",
    "uint",
    "float32",
    "decimal32",
    "decimal64",
    "decimal128"
]

VERSION_MAJOR = PN_VERSION_MAJOR
VERSION_MINOR = PN_VERSION_MINOR
VERSION_POINT = PN_VERSION_POINT
VERSION = (VERSION_MAJOR, VERSION_MINOR, VERSION_POINT)
API_LANGUAGE = "C"
IMPLEMENTATION_LANGUAGE = "C"


handler = logging.NullHandler()

logconfigfile = os.getenv('PNPY_LOGGER_CONFIG', None)
if logconfigfile:
    logging.config.fileConfig(logconfigfile, None, False)
else:
    log = logging.getLogger("proton")
    log.addHandler(handler)
