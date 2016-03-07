#ifndef TYPE_TRAITS_HPP
#define TYPE_TRAITS_HPP

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

/// @cond INTERNAL

/// @file
///
/// Internal: Type traits for mapping between AMQP and C++ types.
///
/// Also provides workarounds for missing type_traits classes on older
/// C++ compilers.

#include "proton/config.hpp"
#include "proton/types.hpp"

namespace proton {

class binary;
class decimal128;
class decimal32;
class decimal64;
class scalar;
class symbol;
class timestamp;
class uuid;
class value;

namespace internal {

class decoder;
class encoder;

template <bool, class T=void> struct enable_if {};
template <class T> struct enable_if<true, T> { typedef T type; };

struct true_type { static const bool value = true; };
struct false_type { static const bool value = false; };

template <class T> struct is_integral : public false_type {};
template <class T> struct is_signed : public false_type {};

template <> struct is_integral<char> : public true_type {};
template <> struct is_signed<char> : public false_type {};

template <> struct is_integral<unsigned char> : public true_type {};
template <> struct is_integral<unsigned short> : public true_type {};
template <> struct is_integral<unsigned int> : public true_type {};
template <> struct is_integral<unsigned long> : public true_type {};

template <> struct is_integral<signed char> : public true_type {};
template <> struct is_integral<signed short> : public true_type {};
template <> struct is_integral<signed int> : public true_type {};
template <> struct is_integral<signed long> : public true_type {};

template <> struct is_signed<unsigned short> : public false_type {};
template <> struct is_signed<unsigned int> : public false_type {};
template <> struct is_signed<unsigned long> : public false_type {};

template <> struct is_signed<signed char> : public true_type {};
template <> struct is_signed<signed short> : public true_type {};
template <> struct is_signed<signed int> : public true_type {};
template <> struct is_signed<signed long> : public true_type {};

#if PN_CPP_HAS_LONG_LONG
template <> struct is_integral<unsigned long long> : public true_type {};
template <> struct is_integral<signed long long> : public true_type {};
template <> struct is_signed<unsigned long long> : public false_type {};
template <> struct is_signed<signed long long> : public true_type {};
#endif

template <class T, class U> struct is_same { static const bool value=false; };
template <class T> struct is_same<T,T> { static const bool value=true; };

template< class T > struct remove_const          { typedef T type; };
template< class T > struct remove_const<const T> { typedef T type; };

template <type_id ID> struct type_id_constant { static const type_id value = ID; };

// Metafunction returning AMQP type for scalar C++ types.
template <class T, class Enable=void> struct type_id_of;
template<> struct type_id_of<bool> : public type_id_constant<BOOLEAN> {};
template<> struct type_id_of<uint8_t> : public type_id_constant<UBYTE> {};
template<> struct type_id_of<int8_t> : public type_id_constant<BYTE> {};
template<> struct type_id_of<uint16_t> : public type_id_constant<USHORT> {};
template<> struct type_id_of<int16_t> : public type_id_constant<SHORT> {};
template<> struct type_id_of<uint32_t> : public type_id_constant<UINT> {};
template<> struct type_id_of<int32_t> : public type_id_constant<INT> {};
template<> struct type_id_of<uint64_t> : public type_id_constant<ULONG> {};
template<> struct type_id_of<int64_t> : public type_id_constant<LONG> {};
template<> struct type_id_of<wchar_t> : public type_id_constant<CHAR> {};
template<> struct type_id_of<timestamp> : public type_id_constant<TIMESTAMP> {};
template<> struct type_id_of<float> : public type_id_constant<FLOAT> {};
template<> struct type_id_of<double> : public type_id_constant<DOUBLE> {};
template<> struct type_id_of<decimal32> : public type_id_constant<DECIMAL32> {};
template<> struct type_id_of<decimal64> : public type_id_constant<DECIMAL64> {};
template<> struct type_id_of<decimal128> : public type_id_constant<DECIMAL128> {};
template<> struct type_id_of<uuid> : public type_id_constant<UUID> {};
template<> struct type_id_of<binary> : public type_id_constant<BINARY> {};
template<> struct type_id_of<std::string> : public type_id_constant<STRING> {};
template<> struct type_id_of<symbol> : public type_id_constant<SYMBOL> {};
template<> struct type_id_of<const char*> : public type_id_constant<STRING> {};

// Metafunction to test if a class has a type_id.
template <class T, class Enable=void> struct has_type_id : public false_type {};
template <class T> struct has_type_id<T, typename enable_if<!!type_id_of<T>::value>::type>  : public true_type {};

// Map arbitrary integral types to known AMQP integral types.
template<size_t SIZE, bool IS_SIGNED> struct integer_type;
template<> struct integer_type<1, true> { typedef int8_t type; };
template<> struct integer_type<2, true> { typedef int16_t type; };
template<> struct integer_type<4, true> { typedef int32_t type; };
template<> struct integer_type<8, true> { typedef int64_t type; };
template<> struct integer_type<1, false> { typedef uint8_t type; };
template<> struct integer_type<2, false> { typedef uint16_t type; };
template<> struct integer_type<4, false> { typedef uint32_t type; };
template<> struct integer_type<8, false> { typedef uint64_t type; };

// True if T is an integer type that does not have an explicit type_id.
template <class T> struct is_unknown_integer {
    static const bool value = !has_type_id<T>::value && is_integral<T>::value;
};

// Types for SFINAE tests.
typedef char yes;
typedef double no;
struct wildcard { wildcard(...); };

namespace is_decodable_impl {   // Protected the world from wildcard operator<<

no operator>>(wildcard, wildcard); // Fallback

template<typename T> struct is_decodable {
    static char test(decoder);
    static double test(...);         // Failed test, no match.
    static decoder& d;
    static T &t;
    static bool const value = (sizeof(test(d >> t)) == sizeof(yes));
};
} // namespace is_decodable_impl


namespace is_encodable_impl {   // Protected the world from wildcard operator<<

no operator<<(wildcard, wildcard); // Fallback

template<typename T> struct is_encodable {
    static yes test(encoder);
    static no test(...);         // Failed test, no match.
    static encoder &e;
    static const T& t;
    static bool const value = sizeof(test(e << t)) == sizeof(yes);
};

} // namespace is_encodable_impl

/// is_encodable<T>::value is true if there is an operator<< for encoder and T.
using is_encodable_impl::is_encodable;

/// Metafunction: is_decodable<T>::value is true if `T t; decoder >> t` is valid.
using is_decodable_impl::is_decodable;


// Start encoding a complex type.
struct start {
    PN_CPP_EXTERN start(type_id type=NULL_TYPE, type_id element=NULL_TYPE, bool described=false, size_t size=0);
    type_id type;            ///< The container type: ARRAY, LIST, MAP or DESCRIBED.
    type_id element;         ///< the element type for array only.
    bool is_described;       ///< true if first value is a descriptor.
    size_t size;             ///< the element count excluding the descriptor (if any)

    /// Return a start for an array.
    PN_CPP_EXTERN static start array(type_id element, bool described=false);

    /// Return a start for a list.
    PN_CPP_EXTERN static start list();

    /// Return a start for a map.
    PN_CPP_EXTERN static start map();

    /// Return a start for a described type.
    PN_CPP_EXTERN static start described();
};

/// Finish inserting or extracting a container value.
struct finish {};

} // namespace internal

/// An enabler template for C++ values that can be converted to AMQP.
template<class T, class U=void> struct enable_amqp_type :
        public internal::enable_if<internal::is_encodable<T>::value, U> {};

/// An enabler for integer types.
template <class T, class U=void> struct enable_integer :
        public internal::enable_if<internal::is_unknown_integer<T>::value, U> {};
} // namespace proton

/// @endcond

#endif // TYPE_TRAITS_HPP
