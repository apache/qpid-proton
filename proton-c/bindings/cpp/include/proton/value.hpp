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

#include <iosfwd>

namespace proton {

namespace internal {

// Separate value data from implicit conversion constructors to avoid template recursion.
class value_base {
  protected:
    internal::data& data();
    internal::data data_;

  friend class value_ref;
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

    /// Construct from any allowed type T.
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

    /// @cond INTERNAL (deprecated)
    template<class T> void get(T &t) const;
    template<class T> T get() const;
    PN_CPP_EXTERN int64_t as_int() const;
    PN_CPP_EXTERN uint64_t as_uint() const;
    PN_CPP_EXTERN double as_double() const;
    PN_CPP_EXTERN std::string as_string() const;
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
};

namespace internal {

// value_ref is a `pn_data_t* p` that can be returned as a value& and used to modify
// the underlying value in-place.
//
// Classes with a value_ref member can return it as a value& in accessor functions.
// It can also be used to copy a pn_data_t* p to a proton::value via: value(value_ref(p));
// None of the constructors make copies, they just refer to the same value.
//
class value_ref : public value {
  public:
    value_ref(pn_data_t* = 0);
    value_ref(const internal::data&);
    value_ref(const value_base&);

    // Use refer() not operator= to avoid confusion with value op=
    void refer(pn_data_t*);
    void refer(const internal::data&);
    void refer(const value_base&);

    // Reset to refer to nothing, release existing references. Equivalent to refer(0).
    void reset();

    // Assignments to value_ref means assigning to the value.
    template <class T> value_ref& operator=(const T& x) {
        static_cast<value&>(*this) = x;
        return *this;
    }
};

}


/// @copydoc scalar::get
/// @related proton::value
template<class T> T get(const value& v) { T x; get(v, x); return x; }

/// Like get(const value&) but assigns the value to a reference
/// instead of returning it.  May be more efficient for complex values
/// (arrays, maps, etc.)
///
/// @related proton::value
template<class T> void get(const value& v, T& x) { codec::decoder d(v, true); d >> x; }

/// @copydoc scalar::coerce
/// @related proton::value
template<class T> T coerce(const value& v) { T x; coerce(v, x); return x; }

/// Like coerce(const value&) but assigns the value to a reference
/// instead of returning it.  May be more efficient for complex values
/// (arrays, maps, etc.)
///
/// @related proton::value
template<class T> void coerce(const value& v, T& x) {
    codec::decoder d(v, false);
    if (type_id_is_scalar(v.type())) {
        scalar s;
        d >> s;
        x = internal::coerce<T>(s);
    } else {
        d >> x;
    }
}

/// Special case for get<null>(), just checks that value contains NULL.
template<> inline void get<null>(const value& v, null&) {
    assert_type_equal(NULL_TYPE, v.type());
}

/// Return a readable string representation of x for display purposes.
PN_CPP_EXTERN std::string to_string(const value& x);

/// @cond INTERNAL
template<class T> void value::get(T &x) const { x = proton::get<T>(*this); }
template<class T> T value::get() const { return proton::get<T>(*this); }
inline int64_t value::as_int() const { return proton::coerce<int64_t>(*this); }
inline uint64_t value::as_uint() const { return proton::coerce<uint64_t>(*this); }
inline double value::as_double() const { return proton::coerce<double>(*this); }
inline std::string value::as_string() const { return proton::coerce<std::string>(*this); }
/// @endcond

} // proton

#endif // PROTON_VALUE_HPP
