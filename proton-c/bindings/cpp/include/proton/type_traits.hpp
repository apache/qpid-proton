#ifndef ENABLE_IF_HPP
#define ENABLE_IF_HPP
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

/**@file
 * Type traits used for type conversions.
 * @internal
 */
#if  defined(__cplusplus) && __cplusplus >= 201100
#include <type_traits>
#else
namespace std {

// Workaround for older C++ compilers. NOTE this is NOT a full implementation of the
// corresponding c++11 types, it is the bare minimum needed by this library.

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
}
#endif // Old C++ workarounds

namespace proton {

// Metafunction returning exact AMQP type associated with a C++ type
template <class T> struct TypeIdOf;
template<> struct TypeIdOf<Null> { static const TypeId value=NULL_; };
template<> struct TypeIdOf<Bool> { static const TypeId value=BOOL; };
template<> struct TypeIdOf<Ubyte> { static const TypeId value=UBYTE; };
template<> struct TypeIdOf<Byte> { static const TypeId value=BYTE; };
template<> struct TypeIdOf<Ushort> { static const TypeId value=USHORT; };
template<> struct TypeIdOf<Short> { static const TypeId value=SHORT; };
template<> struct TypeIdOf<Uint> { static const TypeId value=UINT; };
template<> struct TypeIdOf<Int> { static const TypeId value=INT; };
template<> struct TypeIdOf<Char> { static const TypeId value=CHAR; };
template<> struct TypeIdOf<Ulong> { static const TypeId value=ULONG; };
template<> struct TypeIdOf<Long> { static const TypeId value=LONG; };
template<> struct TypeIdOf<Timestamp> { static const TypeId value=TIMESTAMP; };
template<> struct TypeIdOf<Float> { static const TypeId value=FLOAT; };
template<> struct TypeIdOf<Double> { static const TypeId value=DOUBLE; };
template<> struct TypeIdOf<Decimal32> { static const TypeId value=DECIMAL32; };
template<> struct TypeIdOf<Decimal64> { static const TypeId value=DECIMAL64; };
template<> struct TypeIdOf<Decimal128> { static const TypeId value=DECIMAL128; };
template<> struct TypeIdOf<Uuid> { static const TypeId value=UUID; };
template<> struct TypeIdOf<Binary> { static const TypeId value=BINARY; };
template<> struct TypeIdOf<String> { static const TypeId value=STRING; };
template<> struct TypeIdOf<Symbol> { static const TypeId value=SYMBOL; };

template <class T, class Enable=void> struct HasTypeId { static const bool value = false; };
template <class T> struct HasTypeId<T, typename std::enable_if<TypeIdOf<T>::value>::type>  {
    static const bool value = true;
};

// Map to known integer types by sizeof and signedness.
template<size_t N, bool S> struct IntegerType;
template<> struct IntegerType<1, true> { typedef Byte type; };
template<> struct IntegerType<2, true> { typedef Short type; };
template<> struct IntegerType<4, true> { typedef Int type; };
template<> struct IntegerType<8, true> { typedef Long type; };
template<> struct IntegerType<1, false> { typedef Ubyte type; };
template<> struct IntegerType<2, false> { typedef Ushort type; };
template<> struct IntegerType<4, false> { typedef Uint type; };
template<> struct IntegerType<8, false> { typedef Ulong type; };

// True if T is an integer type that does not have a TypeId mapping.
template <class T> struct IsUnknownInteger {
    static const bool value = !HasTypeId<T>::value && std::is_integral<T>::value;
};

}
#endif // ENABLE_IF_HPP
