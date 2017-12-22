#ifndef PROTON_CODEC_UNORDERED_MAP_HPP
#define PROTON_CODEC_UNORDERED_MAP_HPP

/*
 *
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
 *
 */

/// @file
/// **Unsettled API** - Enable conversions between `proton::value` and `std::unordered_map`.

#include "./encoder.hpp"
#include "./decoder.hpp"

#include <unordered_map>

namespace proton {
namespace codec {

/// Encode std::unordered_map<K, T> as amqp::UNORDERED_MAP.
template <class K, class T, class C, class A>
encoder& operator<<(encoder& e, const std::unordered_map<K, T, C, A>& m) { return e << encoder::map(m); }

/// Decode to std::unordered_map<K, T> from amqp::UNORDERED_MAP.
template <class K, class T, class C, class A>
decoder& operator>>(decoder& d, std::unordered_map<K, T, C, A>& m) { return d >> decoder::associative(m); }

} // codec
} // proton

#endif // PROTON_CODEC_UNORDERED_MAP_HPP
