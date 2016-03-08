#ifndef PROTON_LIST_HPP
#define PROTON_LIST_HPP
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

#include <list>
#include <utility>

#include <proton/encoder.hpp>
#include <proton/decoder.hpp>

namespace proton {
namespace codec {
/// std::list<T> for most T is encoded as an AMQP array.
template <class T, class A>
encoder& operator<<(encoder& e, const std::list<T, A>& x) {
    return e << encoder::array(x, type_id_of<T>::value);
}

/// Specialize for std::list<value>, encode as AMQP list (variable type)
template <class A>
encoder& operator<<(encoder& e, const std::list<value, A>& x) { return e << encoder::list(x); }

/// Specialize for std::list<scalar>, encode as AMQP list (variable type)
template <class A>
encoder& operator<<(encoder& e, const std::list<scalar, A>& x) { return e << encoder::list(x); }

/// Specialize for std::list<std::pair<k,t> >, encode as AMQP map.
/// Allows control over the order of encoding map entries.
template <class A, class K, class T>
encoder& operator<<(encoder& e, const std::list<std::pair<K,T>, A>& x) { return e << encoder::map(x); }

/// Decode to std::list<T> from an amqp::LIST or amqp::ARRAY.
template <class T, class A> decoder& operator>>(decoder& d, std::list<T, A>& x) { return d >> decoder::sequence(x); }

/// Decode to std::list<std::pair<K, T> from an amqp::MAP.
template <class A, class K, class T> decoder& operator>>(decoder& d, std::list<std::pair<K, T> , A>& x) { return d >> decoder::pair_sequence(x); }

}
}

#endif // PROTON_LIST_HPP
