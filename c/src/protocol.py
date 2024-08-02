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
import os
import xml.etree.ElementTree as ET

ns = {'amqp': 'http://www.amqp.org/schema/amqp.xsd'}
doc = ET.parse(os.path.join(os.path.dirname(__file__), "transport.xml")).getroot()
mdoc = ET.parse(os.path.join(os.path.dirname(__file__), "messaging.xml")).getroot()
tdoc = ET.parse(os.path.join(os.path.dirname(__file__), "transactions.xml")).getroot()
sdoc = ET.parse(os.path.join(os.path.dirname(__file__), "security.xml")).getroot()


TYPEStmp = doc.findall("./amqp:section/amqp:type/[@class='composite']", ns) + \
    mdoc.findall("./amqp:section/amqp:type/[@class='composite']", ns) + \
    tdoc.findall("./amqp:section/amqp:type/[@class='composite']", ns) + \
    sdoc.findall("./amqp:section/amqp:type/[@class='composite']", ns) + \
    mdoc.findall("./amqp:section/amqp:type/[@provides='section']", ns)
TYPES = []
for ty in TYPEStmp:
    if ty not in TYPES:
        TYPES.append(ty)
RESTRICTIONS = {}
COMPOSITES = {}

for type in doc.findall("./amqp:section/amqp:type", ns) + mdoc.findall("./amqp:section/amqp:type", ns) + \
        sdoc.findall("./amqp:section/amqp:type", ns) + tdoc.findall("./amqp:section/amqp:type", ns):

    source = type.attrib["source"]
    if source:
        RESTRICTIONS[type.attrib["name"]] = source
    if type.attrib["class"] == "composite":
        COMPOSITES[type.attrib["name"]] = type


def resolve(name):
    if name in RESTRICTIONS:
        return resolve(RESTRICTIONS[name])
    else:
        return name


TYPEMAP = {
    "boolean": ("bool", "", ""),
    "binary": ("pn_binary_t", "*", ""),
    "string": ("wchar_t", "*", ""),
    "symbol": ("char", "*", ""),
    "ubyte": ("uint8_t", "", ""),
    "ushort": ("uint16_t", "", ""),
    "uint": ("uint32_t", "", ""),
    "ulong": ("uint64_t", "", ""),
    "timestamp": ("uint64_t", "", ""),
    "list": ("pn_list_t", "*", ""),
    "map": ("pn_map_t", "*", ""),
    "box": ("pn_box_t", "*", ""),
    "*": ("pn_object_t", "*", "")
}

CONSTRUCTORS = {
    "boolean": "boolean",
    "string": "string",
    "symbol": "symbol",
    "ubyte": "ubyte",
    "ushort": "ushort",
    "uint": "uint",
    "ulong": "ulong",
    "timestamp": "ulong"
}

NULLABLE = set(["string", "symbol"])


def fname(field):
    return field.attrib["name"].replace("-", "_")


def tname(t):
    return t.attrib["name"].replace("-", "_")


def multi(f):
    return f.attrib.get("multiple") == "true"


def ftype(field):
    if multi(field):
        return "list"
    elif field.attrib["type"] in COMPOSITES:
        return "box"
    else:
        return resolve(field.attrib["type"]).replace("-", "_")


def fconstruct(field, expr):
    type = ftype(field)
    if type in CONSTRUCTORS:
        result = "pn_%s(mem, %s)" % (CONSTRUCTORS[type], expr)
        if type in NULLABLE:
            result = "%s ? %s : NULL" % (expr, result)
    else:
        result = expr
    if multi(field):
        result = "pn_box(mem, pn_boolean(mem, true), %s)" % result
    return result


def declaration(field):
    name = fname(field)
    type = ftype(field)
    t, pre, post = TYPEMAP.get(type, (type, "", ""))
    return t, "%s%s%s" % (pre, name, post)


def field_kw(field):
    return fname(field).upper()
