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

#include "proton/cpp/Value.h"
#include "proton_bits.h"
#include <proton/codec.h>
#include <ostream>

namespace proton {
namespace reactor {

Values::Values() {}
Values::Values(const Values& v) { *this = v; }
Values::~Values() {}
Values& Values::operator=(const Values& v) { Data::operator=(v); }

void Values::rewind() { pn_data_rewind(data); }

Value::Value() {}
Value::Value(const Value& v) { *this = v; }
Value::~Value() {}
Value& Value::operator=(const Value& v) { values = v.values; }

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

Encoder& operator<<(Encoder& e, const Value& v) {
    if (e.data == v.values.data) throw Encoder::Error("Values inserted into self");
    pn_data_narrow(e.data);
    int result = pn_data_appendn(e.data, v.values.data, 1);
    pn_data_widen(e.data);
    check(result);
    return e;
}

Decoder& operator>>(Decoder& e, Value& v) {
    if (e.data == v.values.data) throw Decoder::Error("Values extracted from self");
    pn_data_narrow(e.data);
    int result = pn_data_appendn(e.data, v.values.data, 1);
    pn_data_widen(e.data);
    check(result);
    return e;
}

}}
