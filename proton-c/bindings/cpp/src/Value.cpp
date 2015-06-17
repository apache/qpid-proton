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

#include "proton/Value.hpp"
#include "proton_bits.hpp"
#include <proton/codec.h>
#include <ostream>
#include <algorithm>

namespace proton {
namespace reactor {

Value::Value() { *this = Null(); }
Value::Value(const Value& v) { *this = v; }
Value::~Value() {}
Value& Value::operator=(const Value& v) { values = v.values; return *this; }

TypeId Value::type() const {
    const_cast<Values&>(values).rewind();
    return values.type();
}

namespace {
template <class T> T check(T result) {
    if (result < 0)
        throw Encoder::Error("encode: " + errorStr(result));
    return result;
}
}

std::ostream& operator<<(std::ostream& o, const Value& v) {
    return o << v.values;
}

namespace {

// Compare nodes, return -1 if a<b, 0 if a==b, +1 if a>b
// Forward-declare so we can use it recursively.
int compareNext(Values& a, Values& b);

template <class T> int compare(const T& a, const T& b) {
    if (a < b) return -1;
    else if (a > b) return +1;
    else return 0;
}

int compareContainer(Values& a, Values& b) {
    Decoder::Scope sa(a), sb(b);
    // Compare described vs. not-described.
    int cmp = compare(sa.isDescribed, sb.isDescribed);
    if (cmp) return cmp;
    // Lexical sort (including descriptor if there is one)
    size_t minSize = std::min(sa.size, sb.size) + int(sa.isDescribed);
    for (size_t i = 0; i < minSize; ++i) {
        cmp = compareNext(a, b);
        if (cmp) return cmp;
    }
    return compare(sa.size, sb.size);
}

template <class T> int compareSimple(Values& a, Values& b) {
    T va, vb;
    a >> va;
    b >> vb;
    return compare(va, vb);
}

int compareNext(Values& a, Values& b) {
    // Sort by TypeId first.
    TypeId ta = a.type(), tb = b.type();
    int cmp = compare(ta, tb);
    if (cmp) return cmp;

    switch (ta) {
      case NULL_: return 0;
      case ARRAY:
      case LIST:
      case MAP:
      case DESCRIBED:
        return compareContainer(a, b);
      case BOOL: return compareSimple<Bool>(a, b);
      case UBYTE: return compareSimple<Ubyte>(a, b);
      case BYTE: return compareSimple<Byte>(a, b);
      case USHORT: return compareSimple<Ushort>(a, b);
      case SHORT: return compareSimple<Short>(a, b);
      case UINT: return compareSimple<Uint>(a, b);
      case INT: return compareSimple<Int>(a, b);
      case CHAR: return compareSimple<Char>(a, b);
      case ULONG: return compareSimple<Ulong>(a, b);
      case LONG: return compareSimple<Long>(a, b);
      case TIMESTAMP: return compareSimple<Timestamp>(a, b);
      case FLOAT: return compareSimple<Float>(a, b);
      case DOUBLE: return compareSimple<Double>(a, b);
      case DECIMAL32: return compareSimple<Decimal32>(a, b);
      case DECIMAL64: return compareSimple<Decimal64>(a, b);
      case DECIMAL128: return compareSimple<Decimal128>(a, b);
      case UUID: return compareSimple<Uuid>(a, b);
      case BINARY: return compareSimple<Binary>(a, b);
      case STRING: return compareSimple<String>(a, b);
      case SYMBOL: return compareSimple<Symbol>(a, b);
    }
    // Invalid but equal TypeId, treat as equal.
    return 0;
}

} // namespace

bool Value::operator==(const Value& v) const {
    values.rewind();
    v.values.rewind();
    return compareNext(values, v.values) == 0;
}

bool Value::operator<(const Value& v) const {
    values.rewind();
    v.values.rewind();
    return compareNext(values, v.values) < 0;
}

}}
