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

from __future__ import print_function
from protocol import *

print("/* generated */")
print("#ifndef _PROTON_PROTOCOL_H")
print("#define _PROTON_PROTOCOL_H 1")
print()
print("#include \"proton/type_compat.h\"")

fields = {}

for type in TYPES:
    fidx = 0
    for f in type.query["field"]:
        print("#define %s_%s (%s)" % (field_kw(type), field_kw(f), fidx))
        fidx += 1
        d = f["@default"]
        if d:
            ft = ftype(f)
            # Don't bother to emit a boolean default that is False
            if ft == "boolean" and d == "false":
                continue
            # Don't output non numerics unless symbol
            # We should really fully resolve to actual restricted value
            # this is really true for symbols too which accidentally work
            if ft == "symbol":
                d = '"' + d + '"'
            elif d[0] not in '0123456789':
                continue
            print("#define %s_%s_DEFAULT (%s) /* %s */" % (field_kw(type), field_kw(f), d, ft))

idx = 0

for type in TYPES:
    desc = type["descriptor"]
    name = type["@name"].upper().replace("-", "_")
    print("#define %s_SYM (\"%s\")" % (name, desc["@name"]))
    hi, lo = [int(x, 0) for x in desc["@code"].split(":")]
    code = (hi << 32) + lo
    print("#define %s ((uint64_t) %s)" % (name, code))
    fields[code] = (type["@name"], [f["@name"] for f in type.query["field"]])
    idx += 1

print("""
#include <stddef.h>

typedef struct {
  const unsigned char name_index;
  const unsigned char first_field_index;
  const unsigned char field_count;
} pn_fields_t;

extern const pn_fields_t FIELDS[];
extern const uint16_t FIELD_NAME[];
extern const uint16_t FIELD_FIELDS[];
""")

print('struct FIELD_STRINGS {')
print('  const char STRING0[sizeof("")];')
strings = set()
for name, fnames in fields.values():
    strings.add(name)
    strings.update(fnames)
for str in strings:
    istr = str.replace("-", "_")
    print('  const char FIELD_STRINGS_%s[sizeof("%s")];' % (istr, str))
print("};")
print()

print("extern const struct FIELD_STRINGS FIELD_STRINGPOOL;")
print("#ifdef DEFINE_FIELDS")
print()

print('const struct FIELD_STRINGS FIELD_STRINGPOOL = {')
print('  "",')
for str in strings:
    print('  "%s",' % str)
print("};")
print()
print("/* This is an array of offsets into FIELD_STRINGPOOL */")
print("const uint16_t FIELD_NAME[] = {")
print("  offsetof(struct FIELD_STRINGS, STRING0),")
index = 1
for i in range(256):
    if i in fields:
        name, fnames = fields[i]
        iname = name.replace("-", "_")
        print('  offsetof(struct FIELD_STRINGS, FIELD_STRINGS_%s), /* %d */' % (iname, index))
        index += 1
print("};")

print("/* This is an array of offsets into FIELD_STRINGPOOL */")
print("const uint16_t FIELD_FIELDS[] = {")
print("  offsetof(struct FIELD_STRINGS, STRING0),")
index = 1
for i in range(256):
    if i in fields:
        name, fnames = fields[i]
        if fnames:
            for f in fnames:
                ifname = f.replace("-", "_")
                print('  offsetof(struct FIELD_STRINGS, FIELD_STRINGS_%s), /* %d (%s) */' % (ifname, index, name))
                index += 1
print("};")

print("const pn_fields_t FIELDS[] = {")

name_count = 1
field_count = 1
field_min = 256
field_max = 0
for i in range(256):
    if i in fields:
        if i > field_max:
            field_max = i
        if i < field_min:
            field_min = i

for i in range(field_min, field_max + 1):
    if i in fields:
        name, fnames = fields[i]
        if fnames:
            print('  {%d, %d, %d}, /* %d (%s) */' % (name_count, field_count, len(fnames), i, name))
            field_count += len(fnames)
        else:
            print('  {%d, 0, 0}, /* %d (%s) */' % (name_count, i, name))
        name_count += 1
        if i > field_max:
            field_max = i
        if i < field_min:
            field_min = i
    else:
        print('  {0, 0, 0}, /* %d */' % i)

print("};")
print()
print("#endif")
print()
print('enum {')
print('  FIELD_MIN = %d,' % field_min)
print('  FIELD_MAX = %d' % field_max)
print('};')
print()
print("#endif /* protocol.h */")
