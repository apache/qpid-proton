#ifndef PROTON_SCALAR_HPP
#define PROTON_SCALAR_HPP

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

namespace proton {

namespace codec {
class decoder;
class encoder;
}

/// A holder for an instance of any scalar AMQP type, see \ref types.
///
class scalar : public scalar_base {
  public:
    /// Create an empty scalar.
    PN_CPP_EXTERN scalar() {}

    /// Construct from any scalar type, see \ref types.
    template <class T> scalar(const T& x) { *this = x; }

    /// Assign from any scalar type, see \ref types.
    template <class T> scalar& operator=(const T& x) { put_(x); return *this; }

    /// No contents, type() == NULL_TYPE
    bool empty() const { return type() == NULL_TYPE; }

    /// Clear the scalar, make it empty()
    void clear() { *this = null(); }

};

/// Get a contained value of type T. For example:
///
///      uint64_t i = get<uint64_t>(x)
///
/// Will succeed if and only if x contains a uint64_t value.
///
/// @throw conversion_error if contained value is not of type T.
/// @related scalar
template<class T> T get(const scalar& s) { return internal::get<T>(s); }

/// Coerce the contained value to type T. For example:
///
///      uint64_t i = get<uint64_t>(x)
///
/// Will succeed if x contains any numeric value, but may lose precision if it
/// contains a float or double value.
///
/// @throw conversion_error if the value cannot be converted to T according to `std::is_convertible`
/// @related scalar
template<class T> T coerce(const scalar& x) { return internal::coerce<T>(x); }

}

#endif  /*!PROTON_SCALAR_HPP*/
