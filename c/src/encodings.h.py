#!/usr/bin/python
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
xml = os.path.join(os.path.dirname(__file__), "types.xml")
doc = ET.parse(xml).getroot()


print("/* generated from %s */" % xml)
print("#ifndef _PROTON_ENCODINGS_H")
print("#define _PROTON_ENCODINGS_H 1")
print()
print("#define PNE_DESCRIPTOR          (0x00)")

types = doc.findall('./amqp:section/amqp:type', ns)
encodings = [(t.attrib['name'], e) for t in types for e in t.findall('./amqp:encoding', ns)]
for parentname, enc in encodings:
    name = enc.attrib.get("name")
    if not name or name == "ieee-754":
        name = parentname
    cname = "PNE_" + name.replace("-", "_").upper()
    print("#define %s%s(%s)" % (cname, " " * (20 - len(cname)), enc.attrib["code"]))

print()
print("#endif /* encodings.h */")
