#ifndef PROTON_VALUE_HPP
#define PROTON_VALUE_HPP

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

#include "./codec/encoder.hpp"
#include "./codec/decoder.hpp"
#include "./internal/type_traits.hpp"
#include "./scalar.hpp"
#include "./types_fwd.hpp"

#include <proton/type_compat.h>

#include <iosfwd>

/// @file
/// @copybrief proton::value

namespace proton {

namespace internal {

// Separate value data from implicit conversion constructors to avoid template recursion.
class value_base {
  protected:
    internal::data& data();
    internal::data data_;

  friend class codec::encoder;
  friend class codec::decoder;
};

} // internal

/// A holder for any AMQP value, simple or complex.
///
/// @see @ref types_page
class value : public internal::value_base, private internal::comparable<value> {
  private:
    // Enabler for encodable types excluding proton::value.
    template<class T, class U=void> struct assignable :
        public internal::enable_if<codec::is_encodable<T>::value, U> {};
    template<class U> struct assignable<value, U> {};

  public:
    /// Create a null value
    PN_CPP_EXTERN value();

    /// @name Copy a value
    /// @{
    PN_CPP_EXTERN value(const value&);
    PN_CPP_EXTERN value& operator=(const value&);
#if PN_CPP_HAS_RVALUE_REFERENCES
    PN_CPP_EXTERN value(value&&);
    PN_CPP_EXTERN value& operator=(value&&);
#endif
    /// @}

    /// Copy from any allowed type T.
    template <class T> value(const T& x, typename assignable<T>::type* = 0) { *this = x; }

    /// Assign from any allowed type T.
    template <class T> typename assignable<T, value&>::type operator=(const T& x) {
        codec::encoder e(*this);
        e << x;
        return *this;
    }

    /// Get the type ID for the current value.
    PN_CPP_EXTERN type_id type() const;

    /// True if the value is null
    PN_CPP_EXTERN bool empty() const;

    /// Reset the value to null/empty
    PN_CPP_EXTERN void clear();

    /// @cond INTERNAL
    template<class T> PN_CPP_DEPRECATED("Use 'proton::get'") void get(T &t) const;
    template<class T> PN_CPP_DEPRECATED("Use 'proton::get'") T get() const;
    /// @endcond

    /// swap values
  friend PN_CPP_EXTERN void swap(value&, value&);

    /// @name Comparison operators
    /// @{
  friend PN_CPP_EXTERN bool operator==(const value& x, const value& y);
  friend PN_CPP_EXTERN bool operator<(const value& x, const value& y);
    ///@}

    /// If contained value is a scalar type T, print using operator<<(T)
    ///
    /// Complex types are printed in a non-standard human-readable format but
    /// that may change in future so should not be parsed.
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const value&);

    ///@cond INTERNAL
    /// Used to refer to existing pn_data_t* values as proton::value
    value(pn_data_t* d);          // Refer to existing pn_data_t
    void reset(pn_data_t* d = 0); // Refer to a new pn_data_t
    ///@endcond
};

/// @copydoc scalar::get
/// @relatedalso proton::value
template<class T> T get(const value& v) { T x; get(v, x); return x; }

/// Like get(const value&) but extracts the value to a reference @p x
/// instead of returning it.  May be more efficient for complex values
/// (arrays, maps, etc.)
///
/// @relatedalso proton::value
template<class T> void get(const value& v, T& x) { codec::decoder d(v, true); d >> x; }

/// @relatedalso proton::value
template<class T, class U> inline void get(const U& u, T& x) { const value v(u); get(v, x); }

/// @copydoc scalar::coerce
/// @relatedalso proton::value
template<class T> T coerce(const value& v) { T x; coerce(v, x); return x; }

/// Like coerce(const value&) but assigns the value to a reference
/// instead of returning it.  May be more efficient for complex values
/// (arrays, maps, etc.)
///
/// @relatedalso proton::value
template<class T> void coerce(const value& v, T& x) {
    codec::decoder d(v, false);
    scalar s;
    if (type_id_is_scalar(v.type())) {
        d >> s;
        x = internal::coerce<T>(s);
    } else {
        d >> x;
    }
}

/// Special case for null, just checks that value contains NULL.
template<> inline void get<null>(const value& v, null&) {
    assert_type_equal(NULL_TYPE, v.type());
}
#if PN_CPP_HAS_NULLPTR
/// @copybrief get<null>()
template<> inline void get<decltype(nullptr)>(const value& v, decltype(nullptr)&) {
    assert_type_equal(NULL_TYPE, v.type());
}
#endif

/// Return a readable string representation of x for display purposes.
PN_CPP_EXTERN std::string to_string(const value& x);

/// @cond INTERNAL
template<class T> void value::get(T &x) const { x = proton::get<T>(*this); }
template<class T> T value::get() const { return proton::get<T>(*this); }
/// @endcond

} // proton

#endif // PROTON_VALUE_HPP
