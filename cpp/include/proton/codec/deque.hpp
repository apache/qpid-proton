#ifndef PROTON_CODEC_DEQUE_HPP
#define PROTON_CODEC_DEQUE_HPP

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

/// @file
/// **Unsettled API** - Enable conversions between `proton::value` and `std::deque`.

#include "./encoder.hpp"
#include "./decoder.hpp"

#include <deque>
#include <utility>

namespace proton {
namespace codec {

/// std::deque<T> for most T is encoded as an amqp::ARRAY (same type elements)
template <class T, class A>
encoder& operator<<(encoder& e, const std::deque<T, A>& x) {
    return e << encoder::array(x, internal::type_id_of<T>::value);
}

/// std::deque<value> encodes as codec::list_type (mixed type elements)
template <class A>
encoder& operator<<(encoder& e, const std::deque<value, A>& x) { return e << encoder::list(x); }

/// std::deque<scalar> encodes as codec::list_type (mixed type elements)
template <class A>
encoder& operator<<(encoder& e, const std::deque<scalar, A>& x) { return e << encoder::list(x); }

/// std::deque<std::pair<k,t> > encodes as codec::map_type.
/// Map entries are encoded in order they appear in the list.
template <class A, class K, class T>
encoder& operator<<(encoder& e, const std::deque<std::pair<K,T>, A>& x) { return e << encoder::map(x); }

/// Decode to std::deque<T> from an amqp::LIST or amqp::ARRAY.
template <class T, class A> decoder& operator>>(decoder& d, std::deque<T, A>& x) { return d >> decoder::sequence(x); }

/// Decode to std::deque<std::pair<K, T> from an amqp::MAP.
template <class A, class K, class T> decoder& operator>>(decoder& d, std::deque<std::pair<K, T> , A>& x) { return d >> decoder::pair_sequence(x); }

} // codec
} // proton

#endif // PROTON_CODEC_DEQUE_HPP
