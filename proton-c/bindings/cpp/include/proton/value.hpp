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

///@internal - separate value data from implicit conversion constructors to avoid recursions.
class value_base {
  public:

    /// Get the type ID for the current value.
    PN_CPP_EXTERN type_id type() const;

    /// True if the value is null
    PN_CPP_EXTERN bool empty() const;

  protected:
    codec::data& data() const;
    mutable class codec::data data_;

  friend class message;
  friend class codec::encoder;
  friend class codec::decoder;
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const value_base&);
};

/// A holder for any AMQP value, simple or complex, see @ref types.
class value : public value_base, private comparable<value> {
  private:
    // Enabler for encodable types excluding proton::value.
    template<class T, class U=void> struct assignable :
        public internal::enable_if<codec::is_encodable<T>::value, U> {};
    template<class U> struct assignable<value, U> {};

  public:
    /// Create a null value
    PN_CPP_EXTERN value();

    ///@name Copy a value
    ///@{
    PN_CPP_EXTERN value(const value&);
    PN_CPP_EXTERN value& operator=(const value&);
#if PN_CPP_HAS_CPP11
    PN_CPP_EXTERN value(value&&);
    PN_CPP_EXTERN value& operator=(value&&);
#endif
    ///@}

    /// Construct from any allowed type T, see @ref types.
    template <class T> value(const T& x, typename assignable<T>::type* = 0) { *this = x; }

    /// Assign from any allowed type T, see @ref types.
    template <class T> typename assignable<T, value&>::type operator=(const T& x) {
        codec::encoder e(*this);
        e << x;
        return *this;
    }

    /// Reset the value to null
    PN_CPP_EXTERN void clear();

    ///@cond INTERNAL (deprecated)
    template<class T> void get(T &t) const;
    template<class T> T get() const;
    PN_CPP_EXTERN int64_t as_int() const;
    PN_CPP_EXTERN uint64_t as_uint() const;
    PN_CPP_EXTERN double as_double() const;
    PN_CPP_EXTERN std::string as_string() const;
    ///@endcond

    /// swap values
  friend PN_CPP_EXTERN void swap(value&, value&);
    ///@name Comparison operators
    ///@{
  friend PN_CPP_EXTERN bool operator==(const value& x, const value& y);
  friend PN_CPP_EXTERN bool operator<(const value& x, const value& y);
    ///@}

    ///@cond INTERNAL
    PN_CPP_EXTERN explicit value(const codec::data&);
    ///@endcond
};

///@copydoc scalar::get
///@related proton::value
template<class T> T get(const value& v) { T x; get(v, x); return x; }

/// Like get(const value&) but assigns the value to a reference instead of returning it.
/// May be more efficient for complex values (arrays, maps etc.)
///@related proton::value
template<class T> void get(const value& v, T& x) { codec::decoder d(v, true); d >> x; }

///@copydoc scalar::coerce
///@related proton::value
template<class T> T coerce(const value& v) { T x; coerce(v, x); return x; }

/// Like coerce(const value&) but assigns the value to a reference instead of returning it.
/// May be more efficient for complex values (arrays, maps etc.)
///@related proton::value
template<class T> void coerce(const value& v, T& x) { codec::decoder d(v, false); d >> x; }

///@cond INTERNAL
template<> inline void get<null>(const value& v, null&) { assert_type_equal(NULL_TYPE, v.type()); }
template<class T> void value::get(T &x) const { x = proton::get<T>(*this); }
template<class T> T value::get() const { return proton::get<T>(*this); }
inline int64_t value::as_int() const { return proton::coerce<int64_t>(*this); }
inline uint64_t value::as_uint() const { return proton::coerce<uint64_t>(*this); }
inline double value::as_double() const { return proton::coerce<double>(*this); }
inline std::string value::as_string() const { return proton::coerce<std::string>(*this); }
///@endcond

} // proton

#endif // PROTON_VALUE_HPP
