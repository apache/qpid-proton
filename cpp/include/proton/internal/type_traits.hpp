#ifndef PROTON_INTERNAL_TYPE_TRAITS_HPP
#define PROTON_INTERNAL_TYPE_TRAITS_HPP

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

// Type traits for mapping between AMQP and C++ types.
//
// Also provides workarounds for missing type_traits classes on older
// C++ compilers.

#include "../types_fwd.hpp"
#include "../type_id.hpp"

#include <proton/type_compat.h>

#include <limits>
#include <type_traits>

namespace proton {
namespace internal {

class decoder;
class encoder;

struct type_id_unknown {
  static constexpr bool has_id = false;
};

template <type_id ID, class T> struct type_id_constant {
    typedef T type;
    static constexpr type_id value = ID;
    static constexpr bool has_id = true;
};

/// @name Metafunction returning AMQP type for scalar C++ types.
/// @{
template <class T> struct type_id_of : public type_id_unknown {};
template<> struct type_id_of<null> : public type_id_constant<NULL_TYPE, null> {};
template<> struct type_id_of<decltype(nullptr)> : public type_id_constant<NULL_TYPE, null> {};
template<> struct type_id_of<bool> : public type_id_constant<BOOLEAN, bool> {};
template<> struct type_id_of<uint8_t> : public type_id_constant<UBYTE, uint8_t> {};
template<> struct type_id_of<int8_t> : public type_id_constant<BYTE, int8_t> {};
template<> struct type_id_of<uint16_t> : public type_id_constant<USHORT, uint16_t> {};
template<> struct type_id_of<int16_t> : public type_id_constant<SHORT, int16_t> {};
template<> struct type_id_of<uint32_t> : public type_id_constant<UINT, uint32_t> {};
template<> struct type_id_of<int32_t> : public type_id_constant<INT, int32_t> {};
template<> struct type_id_of<uint64_t> : public type_id_constant<ULONG, uint64_t> {};
template<> struct type_id_of<int64_t> : public type_id_constant<LONG, int64_t> {};
template<> struct type_id_of<wchar_t> : public type_id_constant<CHAR, wchar_t> {};
template<> struct type_id_of<float> : public type_id_constant<FLOAT, float> {};
template<> struct type_id_of<double> : public type_id_constant<DOUBLE, double> {};
template<> struct type_id_of<timestamp> : public type_id_constant<TIMESTAMP, timestamp> {};
template<> struct type_id_of<decimal32> : public type_id_constant<DECIMAL32, decimal32> {};
template<> struct type_id_of<decimal64> : public type_id_constant<DECIMAL64, decimal64> {};
template<> struct type_id_of<decimal128> : public type_id_constant<DECIMAL128, decimal128> {};
template<> struct type_id_of<uuid> : public type_id_constant<UUID, uuid> {};
template<> struct type_id_of<std::string> : public type_id_constant<STRING, std::string> {};
template<> struct type_id_of<symbol> : public type_id_constant<SYMBOL, symbol> {};
template<> struct type_id_of<binary> : public type_id_constant<BINARY, binary> {};
/// @}

/// Metafunction to test if a class has a type_id.
template <class T> struct has_type_id {
  static constexpr bool value = type_id_of<T>::has_id;
};

// The known/unknown integer type magic is required because the C++ standard is
// vague a about the equivalence of integral types for overloading. E.g. char is
// sometimes equivalent to signed char, sometimes unsigned char, sometimes
// neither. int8_t or uint8_t may or may not be equivalent to a char type.
// int64_t may or may not be equivalent to long long etc. C++ compilers are also
// allowed to add their own non-standard integer types like __int64, which may
// or may not be equivalent to any of the standard integer types.
//
// The solution is to use a fixed, standard set of integer types that are
// guaranteed to be distinct for overloading (see type_id_of) and to use template
// specialization to convert other integer types to a known integer type with the
// same sizeof and is_signed.

// Map arbitrary integral types to known integral types.
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
    static constexpr bool value = !has_type_id<T>::value && std::is_integral<T>::value;
};

template<class T, class = typename std::enable_if<is_unknown_integer<T>::value>::type>
struct known_integer : public integer_type<sizeof(T), std::is_signed<T>::value> {};

// Helper base for SFINAE templates.
struct sfinae {
    typedef char yes;
    typedef double no;
    struct any_t {
        template < typename T > any_t(T const&);
    };
};

} // internal
} // proton

#endif // PROTON_INTERNAL_TYPE_TRAITS_HPP
