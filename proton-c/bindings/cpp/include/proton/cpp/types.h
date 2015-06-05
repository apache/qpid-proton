#ifndef TYPES_H
#define TYPES_H
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

#include <string>
#include <stdint.h>
#include <proton/codec.h>

namespace proton {
namespace reactor {

/**@file
 *
 * C++ types representing simple AMQP types.
 *
 */


/** Convert pn_bytes_t to string */
std::string str(const pn_bytes_t&);

/** Convert string to pn_bytes_t */
pn_bytes_t bytes(const std::string&);

/** Identifies an AMQP type */
enum TypeId {
    NULL_=PN_NULL,
    BOOL=PN_BOOL,
    UBYTE=PN_UBYTE,
    BYTE=PN_BYTE,
    USHORT=PN_USHORT,
    SHORT=PN_SHORT,
    UINT=PN_UINT,
    INT=PN_INT,
    CHAR=PN_CHAR,
    ULONG=PN_ULONG,
    LONG=PN_LONG,
    TIMESTAMP=PN_TIMESTAMP,
    FLOAT=PN_FLOAT,
    DOUBLE=PN_DOUBLE,
    DECIMAL32=PN_DECIMAL32,
    DECIMAL64=PN_DECIMAL64,
    DECIMAL128=PN_DECIMAL128,
    UUID=PN_UUID,
    BINARY=PN_BINARY,
    STRING=PN_STRING,
    SYMBOL=PN_SYMBOL,
    DESCRIBED=PN_DESCRIBED,
    ARRAY=PN_ARRAY,
    LIST=PN_LIST,
    MAP=PN_MAP
};

/** @defgroup types C++ type definitions for AMQP types.
 *
 * These types are all distinct for overloading purposes and will insert as the
 * corresponding AMQP type with Encoder operator<<.
 *
 * @{
 */
struct Null {};
typedef bool Bool;
typedef uint8_t Ubyte;
typedef int8_t Byte;
typedef uint16_t Ushort;
typedef int16_t Short;
typedef uint32_t Uint;
typedef int32_t Int;
typedef wchar_t Char;
typedef uint64_t Ulong;
typedef int64_t Long;
typedef float Float;
typedef double Double;

///@internal
#define STRING_LIKE(NAME)                                               \
    struct NAME : public std::string{                                   \
        NAME(const std::string& s=std::string()) : std::string(s) {}    \
        NAME(const pn_bytes_t& b) : std::string(str(b)) {}              \
        operator pn_bytes_t() const { return bytes(*this); }            \
    }

/** UTF-8 encoded string */
STRING_LIKE(String);
/** ASCII encoded symbolic name */
STRING_LIKE(Symbol);
/** Binary data */
STRING_LIKE(Binary);

// TODO aconway 2015-06-11: alternative representation of variable-length data
// as pointer to existing buffers.

template <class T> struct Decimal {
    T value;
    Decimal(T v) : value(v) {}
    Decimal& operator=(T v) { value = v; }
    operator T() const { return value; }
};
typedef Decimal<pn_decimal32_t> Decimal32;
typedef Decimal<pn_decimal64_t> Decimal64;
typedef Decimal<pn_decimal128_t> Decimal128;

struct Timestamp {
    pn_timestamp_t milliseconds; ///< Since the epoch 00:00:00 (UTC), 1 January 1970.
    Timestamp(int64_t ms) : milliseconds(ms) {}
    operator pn_timestamp_t() const { return milliseconds; }
};

typedef pn_uuid_t Uuid;

///@}

/** Meta-function to get the type-id from a class */
template <class T> struct TypeIdOf {};
template<> struct TypeIdOf<Null> { static const TypeId value; };
template<> struct TypeIdOf<Bool> { static const TypeId value; };
template<> struct TypeIdOf<Ubyte> { static const TypeId value; };
template<> struct TypeIdOf<Byte> { static const TypeId value; };
template<> struct TypeIdOf<Ushort> { static const TypeId value; };
template<> struct TypeIdOf<Short> { static const TypeId value; };
template<> struct TypeIdOf<Uint> { static const TypeId value; };
template<> struct TypeIdOf<Int> { static const TypeId value; };
template<> struct TypeIdOf<Char> { static const TypeId value; };
template<> struct TypeIdOf<Ulong> { static const TypeId value; };
template<> struct TypeIdOf<Long> { static const TypeId value; };
template<> struct TypeIdOf<Timestamp> { static const TypeId value; };
template<> struct TypeIdOf<Float> { static const TypeId value; };
template<> struct TypeIdOf<Double> { static const TypeId value; };
template<> struct TypeIdOf<Decimal32> { static const TypeId value; };
template<> struct TypeIdOf<Decimal64> { static const TypeId value; };
template<> struct TypeIdOf<Decimal128> { static const TypeId value; };
template<> struct TypeIdOf<Uuid> { static const TypeId value; };
template<> struct TypeIdOf<Binary> { static const TypeId value; };
template<> struct TypeIdOf<String> { static const TypeId value; };
template<> struct TypeIdOf<Symbol> { static const TypeId value; };

/** Return the name of a type. */
std::string typeName(TypeId);

/** Return the name of a type from a class. */
template<class T> std::string typeName() { return typeName(TypeIdOf<T>::value); }

}}

#endif // TYPES_H
