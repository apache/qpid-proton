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

#include "proton_bits.hpp"

#include <proton/binary.hpp>
#include <proton/data.hpp>
#include <proton/decimal.hpp>
#include <proton/encoder.hpp>
#include <proton/message_id.hpp>
#include <proton/symbol.hpp>
#include <proton/timestamp.hpp>
#include <proton/value.hpp>

#include <proton/codec.h>

#include <ostream>

namespace proton {
namespace codec {

data data::create() { return internal::take_ownership(pn_data(0)).get(); }

void data::copy(const data& x) { ::pn_data_copy(pn_object(), x.pn_object()); }

void data::clear() { ::pn_data_clear(pn_object()); }

void data::rewind() { ::pn_data_rewind(pn_object()); }

bool data::empty() const { return ::pn_data_size(pn_object()) == 0; }

void* data::point() const { return pn_data_point(pn_object()); }

void data::restore(void* h) { pn_data_restore(pn_object(), pn_handle_t(h)); }

void data::narrow() { pn_data_narrow(pn_object()); }

void data::widen() { pn_data_widen(pn_object()); }

int data::append(data src) { return pn_data_append(pn_object(), src.pn_object());}

int data::appendn(data src, int limit) { return pn_data_appendn(pn_object(), src.pn_object(), limit);}

bool data::next() { return pn_data_next(pn_object()); }

bool data::prev() { return pn_data_prev(pn_object()); }

std::ostream& operator<<(std::ostream& o, const data& d) {
    state_guard sg(const_cast<data&>(d));
    const_cast<data&>(d).rewind();
    return o << inspectable(d.pn_object());
}

} // codec
} // proton
