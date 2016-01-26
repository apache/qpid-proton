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

#include "proton/types.hpp"
#include "proton/scalar.hpp"

namespace proton {
    
class encoder;
class decoder;

/// A key for use with AMQP annotation maps.
///
/// An annotation_key can contain either a uint64_t or a
/// proton::amqp::amqp_symbol.
class annotation_key : public restricted_scalar {
  public:
    /// Create an empty key.
    annotation_key() { scalar_ = uint64_t(0); }

    /// @name Assignment operators
    ///
    /// Assign a C++ value, deducing the AMQP type().
    ///
    /// @{
    annotation_key& operator=(uint64_t x) { scalar_ = x; return *this; }
    annotation_key& operator=(const amqp_symbol& x) { scalar_ = x; return *this; }
    /// `std::string` is encoded as proton::amqp::amqp_symbol.
    annotation_key& operator=(const std::string& x) { scalar_ = amqp_symbol(x); return *this; }
    /// `char*` is encoded as proton::amqp::amqp_symbol.
    annotation_key& operator=(const char *x) { scalar_ = amqp_symbol(x); return *this; }
    /// @}

    /// A constructor that converts from any type that we can assign
    /// from.
    template <class T> annotation_key(T x) { *this = x; }

    /// @name Get methods
    ///
    /// @{
    void get(uint64_t& x) const { scalar_.get(x); }
    void get(amqp_symbol& x) const { scalar_.get(x); }
    /// @}

    /// Return the value as type T.
    template<class T> T get() const { T x; get(x); return x; }

    /// @cond INTERNAL
    friend PN_CPP_EXTERN encoder operator<<(encoder, const annotation_key&);
    friend PN_CPP_EXTERN decoder operator>>(decoder, annotation_key&);
    friend class message;
    /// @endcond
};

}

#endif // ANNOTATION_KEY_HPP
