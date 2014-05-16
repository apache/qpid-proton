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

from protocol import *

print "/* generated */"
print "#ifndef _PROTON_PROTOCOL_H"
print "#define _PROTON_PROTOCOL_H 1"
print

fields = {}

for type in TYPES:
  fidx = 0
  for f in type.query["field"]:
    print "#define %s_%s (%s)" % (field_kw(type), field_kw(f), fidx)
    fidx += 1

idx = 0

for type in TYPES:
  desc = type["descriptor"]
  name = type["@name"].upper().replace("-", "_")
  print "#define %s_SYM (\"%s\")" % (name, desc["@name"])
  hi, lo = [int(x, 0) for x in desc["@code"].split(":")]
  code = (hi << 32) + lo
  print "#define %s ((uint64_t) %s)" % (name, code)
  fields[code] = (type["@name"], [f["@name"] for f in type.query["field"]])
  idx += 1

print

print """typedef struct {
  const unsigned char name_index;
  const unsigned char first_field_index;
  const unsigned char field_count;
} pn_fields_t;

extern const pn_fields_t FIELDS[256];
extern const char * const FIELD_NAME[];
extern const char * const FIELD_FIELDS[];
"""

print "#ifdef DEFINE_FIELDS"

print "const char * const FIELD_NAME[] = {"
print "  NULL,"
index = 1
for i in range(256):
  if i in fields:
    name, fnames = fields[i]
    print '  "%s", /* %d */' % (name, index)
    index += 1
print "};"

print "const char * const FIELD_FIELDS[] = {"
print "  NULL,"
index = 1
for i in range(256):
  if i in fields:
    name, fnames = fields[i]
    if fnames:
      for f in fnames:
        print '  "%s", /* %d (%s) */' % (f, index, name)
        index += 1
print "};"

print "const pn_fields_t FIELDS[256] = {"

name_count = 1
field_count = 1
for i in range(256):
  if i in fields:
    name, fnames = fields[i]
    if fnames:
      print '  {%d, %d, %d}, /* %d (%s) */' % (name_count, field_count, len(fnames), i, name)
      field_count += len(fnames)
    else:
      print '  {%d, 0, 0}, /* %d (%s) */' % (name_count, i, name)
    name_count += 1
  else:
    print '  {0, 0, 0}, /* %d */' % i

print "};"
print "#endif"

print
print "#endif /* protocol.h */"
