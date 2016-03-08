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

#include "types_internal.hpp"

#include <proton/type_id.hpp>

#include <ostream>

namespace proton {

std::string type_name(type_id t) {
    switch (t) {
      case NULL_TYPE: return "null";
      case BOOLEAN: return "boolean";
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
    }
    return "unknown";
}

std::ostream& operator<<(std::ostream& o, type_id t) { return o << type_name(t); }

void assert_type_equal(type_id want, type_id got) {
    if (want != got) throw make_conversion_error(want, got);
}

} // proton
