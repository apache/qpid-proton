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

#include "proton/cpp/types.h"
#include <proton/codec.h>

namespace proton {
namespace reactor {

const TypeId TypeIdOf<Null>::value = NULL_;
const TypeId TypeIdOf<Bool>::value = BOOL;
const TypeId TypeIdOf<Ubyte>::value = UBYTE;
const TypeId TypeIdOf<Byte>::value = BYTE;
const TypeId TypeIdOf<Ushort>::value = USHORT;
const TypeId TypeIdOf<Short>::value = SHORT;
const TypeId TypeIdOf<Uint>::value = UINT;
const TypeId TypeIdOf<Int>::value = INT;
const TypeId TypeIdOf<Char>::value = CHAR;
const TypeId TypeIdOf<Ulong>::value = ULONG;
const TypeId TypeIdOf<Long>::value = LONG;
const TypeId TypeIdOf<Timestamp>::value = TIMESTAMP;
const TypeId TypeIdOf<Float>::value = FLOAT;
const TypeId TypeIdOf<Double>::value = DOUBLE;
const TypeId TypeIdOf<Decimal32>::value = DECIMAL32;
const TypeId TypeIdOf<Decimal64>::value = DECIMAL64;
const TypeId TypeIdOf<Decimal128>::value = DECIMAL128;
const TypeId TypeIdOf<Uuid>::value = UUID;
const TypeId TypeIdOf<Binary>::value = BINARY;
const TypeId TypeIdOf<String>::value = STRING;
const TypeId TypeIdOf<Symbol>::value = SYMBOL;

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

std::string str(const pn_bytes_t& b) { return std::string(b.start, b.size); }
pn_bytes_t bytes(const std::string& s) { pn_bytes_t b; b.start = &s[0]; b.size = s.size(); return b; }

}}
