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

namespace proton {

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

pn_uuid_t pn_uuid(const std::string& s) {
    pn_uuid_t u = {0};          // Zero initialized.
    std::copy(s.begin(), s.begin() + std::max(s.size(), sizeof(pn_uuid_t::bytes)), &u.bytes[0]);
    return u;
}

Start::Start(TypeId t, TypeId e, bool d, size_t s) : type(t), element(e), isDescribed(d), size(s) {}
Start Start::array(TypeId element, bool described) { return Start(ARRAY, element, described); }
Start Start::list() { return Start(LIST); }
Start Start::map() { return Start(MAP); }
Start Start::described() { return Start(DESCRIBED, NULL_, true); }

}
