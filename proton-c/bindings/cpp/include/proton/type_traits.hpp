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

class value;

template <bool, class T=void> struct enable_if {};
template <class T> struct enable_if<true, T> { typedef T type; };

struct true_type { static const bool value = true; };
struct false_type { static const bool value = false; };

template <class T> struct is_integral : public false_type {};

template <> struct is_integral<unsigned char> : public true_type {};
template <> struct is_integral<unsigned short> : public true_type {};
template <> struct is_integral<unsigned int> : public true_type {};
template <> struct is_integral<unsigned long> : public true_type {};

template <> struct is_integral<signed char> : public true_type {};
template <> struct is_integral<signed short> : public true_type {};
template <> struct is_integral<signed int> : public true_type {};
template <> struct is_integral<signed long> : public true_type {};

template <class T> struct is_signed : public false_type {};

template <> struct is_signed<unsigned char> : public false_type {};
template <> struct is_signed<unsigned short> : public false_type {};
template <> struct is_signed<unsigned int> : public false_type {};
template <> struct is_signed<unsigned long> : public false_type {};

template <> struct is_signed<signed char> : public true_type {};
template <> struct is_signed<signed short> : public true_type {};
template <> struct is_signed<signed int> : public true_type {};
template <> struct is_signed<signed long> : public true_type {};

#if PN_HAS_LONG_LONG
template <> struct is_integral<unsigned long long> : public true_type {};
template <> struct is_integral<signed long long> : public true_type {};
template <> struct is_signed<unsigned long long> : public false_type {};
template <> struct is_signed<signed long long> : public true_type {};
#endif

template <class T, class U> struct is_same { static const bool value=false; };
template <class T> struct is_same<T,T> { static const bool value=true; };


template< class T > struct remove_const          { typedef T type; };
template< class T > struct remove_const<const T> { typedef T type; };

// Metafunction returning AMQP type for scalar C++ types
template <class T, class Enable=void> struct type_id_of;
template<> struct type_id_of<amqp_null> { static const type_id value=NULL_TYPE; };
template<> struct type_id_of<amqp_boolean> { static const type_id value=BOOLEAN; };
template<> struct type_id_of<amqp_ubyte> { static const type_id value=UBYTE; };
template<> struct type_id_of<amqp_byte> { static const type_id value=BYTE; };
template<> struct type_id_of<amqp_ushort> { static const type_id value=USHORT; };
template<> struct type_id_of<amqp_short> { static const type_id value=SHORT; };
template<> struct type_id_of<amqp_uint> { static const type_id value=UINT; };
template<> struct type_id_of<amqp_int> { static const type_id value=INT; };
template<> struct type_id_of<amqp_char> { static const type_id value=CHAR; };
template<> struct type_id_of<amqp_ulong> { static const type_id value=ULONG; };
template<> struct type_id_of<amqp_long> { static const type_id value=LONG; };
template<> struct type_id_of<amqp_timestamp> { static const type_id value=TIMESTAMP; };
template<> struct type_id_of<amqp_float> { static const type_id value=FLOAT; };
template<> struct type_id_of<amqp_double> { static const type_id value=DOUBLE; };
template<> struct type_id_of<amqp_decimal32> { static const type_id value=DECIMAL32; };
template<> struct type_id_of<amqp_decimal64> { static const type_id value=DECIMAL64; };
template<> struct type_id_of<amqp_decimal128> { static const type_id value=DECIMAL128; };
template<> struct type_id_of<amqp_uuid> { static const type_id value=UUID; };
template<> struct type_id_of<amqp_binary> { static const type_id value=BINARY; };
template<> struct type_id_of<amqp_string> { static const type_id value=STRING; };
template<> struct type_id_of<amqp_symbol> { static const type_id value=SYMBOL; };

template <class T, class Enable=void> struct has_type_id : public false_type {};
template <class T> struct has_type_id<T, typename enable_if<!!type_id_of<T>::value>::type>  {
    static const bool value = true;
};

// Map arbitrary integral types to known AMQP integral types.
template<size_t SIZE, bool IS_SIGNED> struct integer_type;
template<> struct integer_type<1, true> { typedef amqp_byte type; };
template<> struct integer_type<2, true> { typedef amqp_short type; };
template<> struct integer_type<4, true> { typedef amqp_int type; };
template<> struct integer_type<8, true> { typedef amqp_long type; };
template<> struct integer_type<1, false> { typedef amqp_ubyte type; };
template<> struct integer_type<2, false> { typedef amqp_ushort type; };
template<> struct integer_type<4, false> { typedef amqp_uint type; };
template<> struct integer_type<8, false> { typedef amqp_ulong type; };

// True if T is an integer type that does not have a type_id mapping.
template <class T> struct is_unknown_integer {
    static const bool value = !has_type_id<T>::value && is_integral<T>::value;
};

}

/// @endcond

#endif // TYPE_TRAITS_HPP
