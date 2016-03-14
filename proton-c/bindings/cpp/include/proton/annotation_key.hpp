#ifndef ANNOTATION_KEY_HPP
#define ANNOTATION_KEY_HPP

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

#include <proton/scalar_base.hpp>
#include <proton/symbol.hpp>

namespace proton {

/// A key for use with AMQP annotation maps.
///
/// An annotation_key can contain either a uint64_t or a proton::symbol.
class annotation_key : public scalar_base {
  public:
    using scalar_base::type;

    /// An empty annotation key has a uint64_t == 0 value.
    annotation_key() { put_(uint64_t(0)); }

    /// Construct from any type that can be assigned
    template <class T> annotation_key(const T& x) { *this = x; }

    ///@name Assign from a uint64_t or symbol.
    ///@{
    annotation_key& operator=(uint64_t x) { put_(x); return *this; }
    annotation_key& operator=(const symbol& x) { put_(x); return *this; }
    ///@}
    ///@name Extra conversions for strings, treated as amqp::SYMBOL.
    ///@{
    annotation_key& operator=(const std::string& x) { put_(symbol(x)); return *this; }
    annotation_key& operator=(const char *x) { put_(symbol(x)); return *this; }
    ///@}

    ///@cond INTERNAL
  friend class message;
  friend class codec::decoder;
    ///@endcond
};

///@cond internal
template <class T> T get(const annotation_key& x);
///@endcond

/// Get the uint64_t value or throw conversion_error. @related annotation_key
template<> inline uint64_t get<uint64_t>(const annotation_key& x) { return internal::get<uint64_t>(x); }
/// Get the @ref symbol value or throw conversion_error. @related annotation_key
template<> inline symbol get<symbol>(const annotation_key& x) { return internal::get<symbol>(x); }
/// Get the @ref binary value or throw conversion_error. @related annotation_key

/// @copydoc scalar::coerce
/// @related annotation_key
template<class T> T coerce(const annotation_key& x) { return internal::coerce<T>(x); }
}

#endif // ANNOTATION_KEY_HPP
