#ifndef PROTON_VALUE_HPP
#define PROTON_VALUE_HPP

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

#include <proton/encoder.hpp>
#include <proton/decoder.hpp>
#include <proton/type_traits.hpp>
#include <proton/types_fwd.hpp>

#include <iosfwd>

namespace proton {

/// A holder for any single AMQP value, simple or complex (can be list, array, map etc.)
///
/// @see proton::amqp for conversions rules between C++ and AMQP types.
///
class value : private comparable<value> {
  public:
    /// Create a null value.
    PN_CPP_EXTERN value();
    /// Create a null value.
    PN_CPP_EXTERN value(const null&);

    /// Copy a value.
    PN_CPP_EXTERN value(const value&);
    PN_CPP_EXTERN value& operator=(const value&);
#if PN_CPP_HAS_CPP11
    PN_CPP_EXTERN value(value&&);
    PN_CPP_EXTERN value& operator=(value&&);
#endif

    ///@internal
    PN_CPP_EXTERN explicit value(const codec::data&);

    /// Construct from any allowed type T. @see proton::amqp for allowed types.
    template <class T> value(const T& x, typename codec::assignable<T>::type* = 0) {
        codec::encoder e(*this);
        e << x;
    }
    PN_CPP_EXTERN value& operator=(const null&);

    /// Assign from any encodable type T. @see proton::amqp for encodable types.
    template <class T>
    typename codec::assignable<T, value&>::type operator=(const T& x) {
        codec::encoder e(*this);
        e << x;
        return *this;
    }

    /// Reset the value to null
    PN_CPP_EXTERN void clear();

    /// True if the value is null
    PN_CPP_EXTERN bool empty() const;

    /// Get the type ID for the current value.
    PN_CPP_EXTERN type_id type() const;

    /// @name Get methods
    ///
    /// Extract the value to type T.
    ///
    /// @{

    /// Get the value.
    template<class T> void get(T &t) const { codec::decoder d(*this); d >> t; }

    PN_CPP_EXTERN void get(null&) const;
    /// @}

    /// Get the value as C++ type T.
    template<class T> T get() const { T t; get(t); return t; }

    /// @name As methods
    ///
    /// As methods do "loose" conversion, they will convert the scalar
    /// value to the requested type if possible, else throw conversion_error.
    ///
    /// @{
    PN_CPP_EXTERN int64_t as_int() const;        ///< Allowed if `type_id_is_integral(type())`
    PN_CPP_EXTERN uint64_t as_uint() const;      ///< Allowed if `type_id_is_integral(type())`
    PN_CPP_EXTERN double as_double() const;      ///< Allowed if `type_id_is_floating_point(type())`
    PN_CPP_EXTERN std::string as_string() const; ///< Allowed if `type_id_is_string_like(type())`
    /// @}

  friend PN_CPP_EXTERN void swap(value&, value&);
  friend PN_CPP_EXTERN bool operator==(const value& x, const value& y);
  friend PN_CPP_EXTERN bool operator<(const value& x, const value& y);
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, codec::exact_cref<value>);

  private:
    codec::data& data() const;
    mutable class codec::data data_;

  friend class message;
  friend class codec::encoder;
  friend class codec::decoder;
};

template<class T> T get(codec::exact_cref<value> v) { T x; v.ref.get(x); return x; }

} // proton

#endif // PROTON_VALUE_HPP
