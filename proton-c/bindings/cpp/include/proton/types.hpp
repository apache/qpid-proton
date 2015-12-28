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

/**@file
 * Defines C++ types representing AMQP types.
 */

#include "proton/export.hpp"
#include "proton/error.hpp"
#include "proton/comparable.hpp"

#include <proton/codec.h>
#include <proton/type_compat.h>

#include <algorithm>
#include <bitset>
#include <string>
#include <memory.h>
#include <algorithm>

namespace proton {

/** type_id identifies an AMQP type. */
enum type_id {
    NULL_TYPE=PN_NULL,          ///< The null type, contains no data.
    BOOLEAN=PN_BOOL,            ///< Boolean true or false.
    UBYTE=PN_UBYTE,             ///< Unsigned 8 bit integer.
    BYTE=PN_BYTE,               ///< Signed 8 bit integer.
    USHORT=PN_USHORT,           ///< Unsigned 16 bit integer.
    SHORT=PN_SHORT,             ///< Signed 16 bit integer.
    UINT=PN_UINT,               ///< Unsigned 32 bit integer.
    INT=PN_INT,                 ///< Signed 32 bit integer.
    CHAR=PN_CHAR,               ///< 32 bit unicode character.
    ULONG=PN_ULONG,             ///< Unsigned 64 bit integer.
    LONG=PN_LONG,               ///< Signed 64 bit integer.
    TIMESTAMP=PN_TIMESTAMP,     ///< Signed 64 bit milliseconds since the epoch.
    FLOAT=PN_FLOAT,             ///< 32 bit binary floating point.
    DOUBLE=PN_DOUBLE,           ///< 64 bit binary floating point.
    DECIMAL32=PN_DECIMAL32,     ///< 32 bit decimal floating point.
    DECIMAL64=PN_DECIMAL64,     ///< 64 bit decimal floating point.
    DECIMAL128=PN_DECIMAL128,   ///< 128 bit decimal floating point.
    UUID=PN_UUID,               ///< 16 byte UUID.
    BINARY=PN_BINARY,           ///< Variable length sequence of bytes.
    STRING=PN_STRING,           ///< Variable length utf8-encoded string.
    SYMBOL=PN_SYMBOL,           ///< Variable length encoded string.
    DESCRIBED=PN_DESCRIBED,     ///< A descriptor and a value.
    ARRAY=PN_ARRAY,             ///< A sequence of values of the same type.
    LIST=PN_LIST,               ///< A sequence of values, may be of mixed types.
    MAP=PN_MAP                  ///< A sequence of key:value pairs, may be of mixed types.
};

/// Raised when there is a type mismatch, with the expected and actual type ID.
struct type_mismatch : public error {
    PN_CPP_EXTERN explicit type_mismatch(type_id want, type_id got, const std::string& =std::string());
    type_id want; ///< Expected type_id
    type_id got;  ///< Actual type_id
};

PN_CPP_EXTERN pn_bytes_t pn_bytes(const std::string&);
PN_CPP_EXTERN std::string str(const pn_bytes_t& b);
///@endcond

/// AMQP NULL type.
struct amqp_null {};
/// AMQP boolean type.
typedef bool amqp_boolean;
/// AMQP unsigned 8-bit type.
typedef ::uint8_t amqp_ubyte;
/// AMQP signed 8-bit integer type.
typedef ::int8_t amqp_byte;
/// AMQP unsigned 16-bit integer type.
typedef ::uint16_t amqp_ushort;
/// AMQP signed 16-bit integer type.
typedef ::int16_t amqp_short;
/// AMQP unsigned 32-bit integer type.
typedef ::uint32_t amqp_uint;
/// AMQP signed 32-bit integer type.
typedef ::int32_t amqp_int;
/// AMQP 32-bit unicode character type.
typedef wchar_t amqp_char;
/// AMQP unsigned 64-bit integer type.
typedef ::uint64_t amqp_ulong;
/// AMQP signed 64-bit integer type.
typedef ::int64_t amqp_long;
/// AMQP 32-bit floating-point type.
typedef float amqp_float;
/// AMQP 64-bit floating-point type.
typedef double amqp_double;

/// AMQP UTF-8 encoded string.
struct amqp_string : public std::string {
    amqp_string(const std::string& s=std::string()) : std::string(s) {}
    amqp_string(const char* s) : std::string(s) {}
    amqp_string(const pn_bytes_t& b) : std::string(b.start, b.size) {}
    operator pn_bytes_t() const { return pn_bytes(*this); }
};

/// AMQP ASCII encoded symbolic name.
struct amqp_symbol : public std::string {
    amqp_symbol(const std::string& s=std::string()) : std::string(s) {}
    amqp_symbol(const char* s) : std::string(s) {}
    amqp_symbol(const pn_bytes_t& b) : std::string(b.start, b.size) {}
    operator pn_bytes_t() const { return pn_bytes(*this); }
};

/// AMQP variable-length binary data.
struct amqp_binary : public std::string {
    amqp_binary(const std::string& s=std::string()) : std::string(s) {}
    amqp_binary(const char* s) : std::string(s) {}
    amqp_binary(const pn_bytes_t& b) : std::string(b.start, b.size) {}
    operator pn_bytes_t() const { return pn_bytes(*this); }
};

/// Template for opaque proton proton types that can be treated as byte arrays.
template <class P> struct opaque: public comparable<opaque<P> > {
    P value;
    opaque(const P& p=P()) : value(p) {}
    operator P() const { return value; }

    static size_t size() { return sizeof(P); }
    char* begin() { return reinterpret_cast<char*>(&value); }
    char* end() { return reinterpret_cast<char*>(&value)+size(); }
    const char* begin() const { return reinterpret_cast<const char*>(&value); }
    const char* end() const { return reinterpret_cast<const char*>(&value)+size(); }
    char& operator[](size_t i) { return *(begin()+i); }
    const char& operator[](size_t i) const { return *(begin()+i); }

    bool operator==(const opaque& x) const { return std::equal(begin(), end(), x.begin()); }
    bool operator<(const opaque& x) const { return std::lexicographical_compare(begin(), end(), x.begin(), x.end()); }
};

/// AMQP 16-byte UUID.
typedef opaque<pn_uuid_t> amqp_uuid;
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const amqp_uuid&);
/// AMQP 32-bit decimal floating point (IEEE 854).
typedef opaque<pn_decimal32_t> amqp_decimal32;
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const amqp_decimal32&);
/// AMQP 64-bit decimal floating point (IEEE 854).
typedef opaque<pn_decimal64_t> amqp_decimal64;
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const amqp_decimal64&);
/// AMQP 128-bit decimal floating point (IEEE 854).
typedef opaque<pn_decimal128_t> amqp_decimal128;
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const amqp_decimal128&);

/// AMQP timestamp, milliseconds since the epoch 00:00:00 (UTC), 1 January 1970.
struct amqp_timestamp : public comparable<amqp_timestamp> {
    pn_timestamp_t milliseconds;
    amqp_timestamp(::int64_t ms=0) : milliseconds(ms) {}
    operator pn_timestamp_t() const { return milliseconds; }
    bool operator==(const amqp_timestamp& x) { return milliseconds == x.milliseconds; }
    bool operator<(const amqp_timestamp& x) { return milliseconds < x.milliseconds; }
};
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const amqp_timestamp&);

///@cond INTERNAL
template<class T, type_id A> struct cref {
    typedef T cpp_type;
    static const type_id type;

    cref(const T& v) : value(v) {}
    const T& value;
};
template <class T, type_id A> const type_id cref<T, A>::type = A;
///@endcond INTERNAL

/**
 * Indicate the desired AMQP type to use when encoding T.
 * For example to encode a vector as a list:
 *
 *     std::vector<amqp_int> v;
 *     encoder << as<LIST>(v);
 */
template <type_id A, class T> cref<T, A> as(const T& value) { return cref<T, A>(value); }

// TODO aconway 2015-06-16: described types.

/// Name of the AMQP type
PN_CPP_EXTERN std::string type_name(type_id);

///@name Attributes of a type_id value, returns same result as the
/// corresponding std::type_traits tests for the corresponding C++ types.
///@{
PN_CPP_EXTERN bool type_id_atom(type_id);
PN_CPP_EXTERN bool type_id_integral(type_id);
PN_CPP_EXTERN bool type_id_signed(type_id); ///< CHAR and BOOL are not signed.
PN_CPP_EXTERN bool type_id_floating_point(type_id);
PN_CPP_EXTERN bool type_id_decimal(type_id);
PN_CPP_EXTERN bool type_id_string_like(type_id);   ///< STRING, SYMBOL, BINARY
PN_CPP_EXTERN bool type_id_container(type_id);
///@}

/** Print the name of a type. */
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, type_id);

/** Information needed to start extracting or inserting a container type.
 *
 * See encoder::operator<<(encoder&, const start&) and decoder::operator>>(decoder&, start&)
 * for examples of use.
 */
struct start {
    PN_CPP_EXTERN start(type_id type=NULL_TYPE, type_id element=NULL_TYPE, bool described=false, size_t size=0);
    type_id type;            ///< The container type: ARRAY, LIST, MAP or DESCRIBED.
    type_id element;         ///< the element type for array only.
    bool is_described;       ///< true if first value is a descriptor.
    size_t size;             ///< the element count excluding the descriptor (if any)

    /** Return a start for an array */
    PN_CPP_EXTERN static start array(type_id element, bool described=false);
    /** Return a start for a list */
    PN_CPP_EXTERN static start list();
    /** Return a start for a map */
    PN_CPP_EXTERN static start map();
    /** Return a start for a described type */
    PN_CPP_EXTERN static start described();
};

/** Finish inserting or extracting a container value. */
struct finish {};

}

#endif // TYPES_H
