#ifndef PROTON_ENCODER_HPP
#define PROTON_ENCODER_HPP

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

#include <proton/data.hpp>
#include <proton/types_fwd.hpp>
#include <proton/type_traits.hpp>

namespace proton {

class scalar_base;
class value_base;

/// @ingroup codec
namespace codec {

/// Stream-like encoder from AMQP bytes to C++ values.
///
/// Internal use only, see proton::value, proton::scalar and \ref types
/// for the recommended ways to manage AMQP data.
class encoder : public data {
  public:
    /// Wrap Proton-C data object.
    explicit encoder(const data& d) : data(d) {}

    /// Encoder into v. Clears any current value in v.
    PN_CPP_EXTERN explicit encoder(value_base& v);

    /**
     * Encode the current values into buffer and update size to reflect the
     * number of bytes encoded.
     *
     * Clears the encoder.
     *
     *@return if buffer==0 or size is too small then return false and  size to the required size.
     *Otherwise return true and set size to the number of bytes encoded.
     */
    PN_CPP_EXTERN bool encode(char* buffer, size_t& size);

    /** Encode the current values into a std::string, resize the string if necessary.
     *
     * Clears the encoder.
     */
    PN_CPP_EXTERN void encode(std::string&);

    /** Encode the current values into a std::string. Clears the encoder. */
    PN_CPP_EXTERN std::string encode();

    /// @name Insert built-in types
    /// @{
    PN_CPP_EXTERN encoder& operator<<(bool);
    PN_CPP_EXTERN encoder& operator<<(uint8_t);
    PN_CPP_EXTERN encoder& operator<<(int8_t);
    PN_CPP_EXTERN encoder& operator<<(uint16_t);
    PN_CPP_EXTERN encoder& operator<<(int16_t);
    PN_CPP_EXTERN encoder& operator<<(uint32_t);
    PN_CPP_EXTERN encoder& operator<<(int32_t);
    PN_CPP_EXTERN encoder& operator<<(wchar_t);
    PN_CPP_EXTERN encoder& operator<<(uint64_t);
    PN_CPP_EXTERN encoder& operator<<(int64_t);
    PN_CPP_EXTERN encoder& operator<<(timestamp);
    PN_CPP_EXTERN encoder& operator<<(float);
    PN_CPP_EXTERN encoder& operator<<(double);
    PN_CPP_EXTERN encoder& operator<<(decimal32);
    PN_CPP_EXTERN encoder& operator<<(decimal64);
    PN_CPP_EXTERN encoder& operator<<(decimal128);
    PN_CPP_EXTERN encoder& operator<<(const uuid&);
    PN_CPP_EXTERN encoder& operator<<(const std::string&);
    PN_CPP_EXTERN encoder& operator<<(const symbol&);
    PN_CPP_EXTERN encoder& operator<<(const binary&);
    PN_CPP_EXTERN encoder& operator<<(const scalar_base&);
    PN_CPP_EXTERN encoder& operator<<(const null&);
    ///@}

    /// Insert a proton::value.
    /// @internal NOTE insert value_base, not value to avoid recursive implicit conversions.
    PN_CPP_EXTERN encoder& operator<<(const value_base&);

    /// Start a complex type
    PN_CPP_EXTERN encoder& operator<<(const start&);
    /// Finish a complex type
    PN_CPP_EXTERN encoder& operator<<(const finish&);

    ///@cond INTERNAL

    // Undefined template to  prevent pointers being implicitly converted to bool.
    template <class T> void* operator<<(const T*);

    template <class T> struct list_cref { T& ref; list_cref(T& r) : ref(r) {} };
    template <class T> struct map_cref { T& ref;  map_cref(T& r) : ref(r) {} };

    template <class T> struct array_cref {
        start array_start;
        T& ref;
        array_cref(T& r, type_id el, bool described) : array_start(ARRAY, el, described), ref(r) {}
    };

    template <class T> static list_cref<T> list(T& x) { return list_cref<T>(x); }
    template <class T> static map_cref<T> map(T& x) { return map_cref<T>(x); }
    template <class T> static array_cref<T> array(T& x, type_id element, bool described=false) {
        return array_cref<T>(x, element, described);
    }

    template <class T> encoder& operator<<(const map_cref<T>& x) {
        state_guard sg(*this);
        *this << start::map();
        for (typename T::const_iterator i = x.ref.begin(); i != x.ref.end(); ++i)
            *this << i->first << i->second;
        *this << finish();
        return *this;
    }

    template <class T> encoder& operator<<(const list_cref<T>& x) {
        state_guard sg(*this);
        *this << start::list();
        for (typename T::const_iterator i = x.ref.begin(); i != x.ref.end(); ++i)
            *this << *i;
        *this << finish();
        return *this;
    }

    template <class T> encoder& operator<<(const array_cref<T>& x) {
        state_guard sg(*this);
        *this << x.array_start;
        for (typename T::const_iterator i = x.ref.begin(); i != x.ref.end(); ++i)
            *this << *i;
        *this << finish();
        return *this;
    }
    ///@endcond

  private:
    template<class T, class U> encoder& insert(const T& x, int (*put)(pn_data_t*, U));
    void check(long result);
};


/// Treat char* as string
inline encoder& operator<<(encoder& e, const char* s) { return e << std::string(s); }

/// operator << for integer types that are not covered by the standard overrides.
template <class T> typename internal::enable_if<internal::is_unknown_integer<T>::value, encoder&>::type
operator<<(encoder& e, T i)  {
    using namespace internal;
    return e << static_cast<typename integer_type<sizeof(T), is_signed<T>::value>::type>(i);
}

///@cond INTERNAL
namespace is_encodable_impl {   // Protected the world from wildcard operator<<

using namespace internal;

sfinae::no operator<<(sfinae::wildcard, sfinae::wildcard); // Fallback

template<typename T> struct is_encodable : public sfinae {
    static yes test(encoder);
    static no test(...);         // Failed test, no match.
    static encoder &e;
    static const T& t;
    static bool const value = sizeof(test(e << t)) == sizeof(yes);
};
// Avoid recursion
template <> struct is_encodable<value> : public true_type {};

} // namespace is_encodable_impl

using is_encodable_impl::is_encodable;
///@endcond

} // codec
} // proton

#endif  /*!PROTON_ENCODER_HPP*/
