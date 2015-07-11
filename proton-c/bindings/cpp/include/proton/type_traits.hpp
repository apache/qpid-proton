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

///@cond INTERNAL_DETAIL

#include "proton/types.hpp"

#if  defined(__cplusplus) && __cplusplus >= 201100
#include <type_traits>
#elif defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 150030729
#include <type_traits>
#else
/**
 * Workaround missing std:: classes on older C++ compilers.  NOTE: this is NOT a
 * full implementation of the standard c++11 types, it is the bare minimum
 * needed by this library.
 */
namespace std {


template <bool, class T=void> struct enable_if;
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

#if PN_HAVE_LONG_LONG
template <> struct is_integral<unsigned long long> : public true_type {};
template <> struct is_integral<signed long long> : public true_type {};
template <> struct is_signed<unsigned long long> : public false_type {};
template <> struct is_signed<signed long long> : public true_type {};
#endif
}
#endif // Old C++ workarounds

namespace proton {

// Metafunction returning exact AMQP type associated with a C++ type
template <class T> struct type_id_of;
template<> struct type_id_of<amqp_null> { static const type_id value=NULl_; };
template<> struct type_id_of<amqp_bool> { static const type_id value=BOOL; };
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

template <class T, class Enable=void> struct has_type_id { static const bool value = false; };
template <class T> struct has_type_id<T, typename std::enable_if<type_id_of<T>::value>::type>  {
    static const bool value = true;
};

// amqp_map to known integer types by sizeof and signedness.
template<size_t N, bool S> struct integer_type;
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
    static const bool value = bool((!has_type_id<T>::value) && std::is_integral<T>::value);
};

}

///@endcond

#endif // TYPE_TRAITS_HPP
