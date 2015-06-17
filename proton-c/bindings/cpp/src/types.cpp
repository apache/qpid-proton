/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "proton/types.hpp"
#include <proton/codec.h>
#include <ostream>
#include <algorithm>

namespace proton {

Uuid::Uuid() { std::fill(bytes, bytes+SIZE, 0); }
Uuid::Uuid(const pn_uuid_t& u) { std::copy(u.bytes, u.bytes+SIZE, bytes); }

Uuid::operator pn_uuid_t() const {
    pn_uuid_t u;
    std::copy(begin(), end(), u.bytes);
    return u;
}

bool Uuid::operator==(const Uuid& x) const {
    return std::equal(begin(), end(), x.begin());
}

bool Uuid::operator<(const Uuid& x) const {
    return std::lexicographical_compare(begin(), end(), x.begin(), x.end()) < 0;
}

namespace {
inline std::ostream& printSegment(std::ostream& o, const Uuid& u, size_t begin, size_t end, const char* sep="") {
    for (const char* p = &u[begin]; p < &u[end]; ++p) o << *p;
    return o << sep;
}
}

std::ostream& operator<<(std::ostream& o, const Uuid& u) {
    std::ios_base::fmtflags ff = o.flags();
    o.flags(std::ios_base::hex);
    printSegment(o, u, 0, 4, "-");
    printSegment(o, u, 4, 6, "-");
    printSegment(o, u, 6, 8, "-");
    printSegment(o, u, 8, 10, "-");
    printSegment(o, u, 10, 16);
    o.flags(ff);
    return o;
}

std::string typeName(TypeId t) {
    switch (t) {
      case NULL_: return "null";
      case BOOL: return "bool";
      case UBYTE: return "ubyte";
      case BYTE: return "byte";
      case USHORT: return "ushort";
      case SHORT: return "short";
      case UINT: return "uint";
      case INT: return "int";
      case CHAR: return "char";
      case ULONG: return "ulong";
      case LONG: return "long";
      case TIMESTAMP: return "timestamp";
      case FLOAT: return "float";
      case DOUBLE: return "double";
      case DECIMAL32: return "decimal32";
      case DECIMAL64: return "decimal64";
      case DECIMAL128: return "decimal128";
      case UUID: return "uuid";
      case BINARY: return "binary";
      case STRING: return "string";
      case SYMBOL: return "symbol";
      case DESCRIBED: return "described";
      case ARRAY: return "array";
      case LIST: return "list";
      case  MAP: return "map";
      default: return "unknown";
    }
}

std::ostream& operator<<(std::ostream& o,TypeId t) { return o << typeName(t); }

PN_CPP_EXTERN bool isContainer(TypeId t) {
    return (t == LIST || t == MAP || t == ARRAY || t == DESCRIBED);
}

pn_bytes_t pn_bytes(const std::string& s) {
    pn_bytes_t b = { s.size(), const_cast<char*>(&s[0]) };
    return b;
}

std::string str(const pn_bytes_t& b) { return std::string(b.start, b.size); }

Start::Start(TypeId t, TypeId e, bool d, size_t s) : type(t), element(e), isDescribed(d), size(s) {}
Start Start::array(TypeId element, bool described) { return Start(ARRAY, element, described); }
Start Start::list() { return Start(LIST); }
Start Start::map() { return Start(MAP); }
Start Start::described() { return Start(DESCRIBED, NULL_, true); }

}
