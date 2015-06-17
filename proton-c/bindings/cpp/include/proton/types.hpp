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

#include <proton/codec.h>
#include "proton/ImportExport.hpp"
#include <algorithm>
#include <bitset>
#include <string>
#include <stdint.h>
#include <memory.h>

/**@file
 * C++ types representing AMQP types.
 * @ingroup cpp
 */

namespace proton {


/** TypeId identifies an AMQP type */
enum TypeId {
    NULL_=PN_NULL,              ///< The null type, contains no data.
    BOOL=PN_BOOL,               ///< Boolean true or false.
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

///@internal
template <class T> struct Comparable {};
template<class T> bool operator<(const Comparable<T>& a, const Comparable<T>& b) {
    return static_cast<const T&>(a) < static_cast<const T&>(b); // operator < provided by type T
}
template<class T> bool operator>(const Comparable<T>& a, const Comparable<T>& b) { return b < a; }
template<class T> bool operator<=(const Comparable<T>& a, const Comparable<T>& b) { return !(a > b); }
template<class T> bool operator>=(const Comparable<T>& a, const Comparable<T>& b) { return !(a < b); }
template<class T> bool operator==(const Comparable<T>& a, const Comparable<T>& b) { return a <= b && b <= a; }
template<class T> bool operator!=(const Comparable<T>& a, const Comparable<T>& b) { return !(a == b); }

/**
 * @name C++ types representing AMQP types.
 * @{
 * @ingroup cpp
 * These types are all distinct for overloading purposes and will insert as the
 * corresponding AMQP type with Encoder operator<<.
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
pn_bytes_t pn_bytes(const std::string&);
//@internal
std::string str(const pn_bytes_t& b);

///@internal
#define STRING_LIKE(NAME)                                               \
    PN_CPP_EXTERN struct NAME : public std::string{                     \
        NAME(const std::string& s=std::string()) : std::string(s) {}    \
        NAME(const char* s) : std::string(s) {}    \
        NAME(const pn_bytes_t& b) : std::string(b.start, b.size) {}     \
        operator pn_bytes_t() const { return pn_bytes(*this); }         \
    }

/** UTF-8 encoded string */
STRING_LIKE(String);
/** ASCII encoded symbolic name */
STRING_LIKE(Symbol);
/** Binary data */
STRING_LIKE(Binary);

///@internal
pn_uuid_t pn_uuid(const std::string&);

/** UUID is represented as a string but treated as if it always has 16 bytes. */
PN_CPP_EXTERN struct Uuid : public std::string{
    Uuid(const std::string& s=std::string()) : std::string(s) {}
    Uuid(const pn_uuid_t& u) : std::string(&u.bytes[0], sizeof(pn_uuid_t::bytes)) {}
    operator pn_uuid_t() const { return pn_uuid(*this); }
};

// TODO aconway 2015-06-11: alternative representation of variable-length data
// as pointer to existing buffers.

// TODO aconway 2015-06-16: usable representation of decimal types.
template <class T> struct Decimal : public Comparable<Decimal<T> > {
    char value[sizeof(T)];
    Decimal() { ::memset(value, 0, sizeof(T)); }
    Decimal(const T& v) { ::memcpy(value, &v, sizeof(T)); }
    operator T() const { return *reinterpret_cast<const T*>(value); }
    bool operator<(const Decimal<T>& x) {
        return std::lexicographical_compare(value, value+sizeof(T), x.value, x.value+sizeof(T));
    }
};
typedef Decimal<pn_decimal32_t> Decimal32;
typedef Decimal<pn_decimal64_t> Decimal64;
typedef Decimal<pn_decimal128_t> Decimal128;

PN_CPP_EXTERN struct Timestamp : public Comparable<Timestamp> {
    pn_timestamp_t milliseconds; ///< Since the epoch 00:00:00 (UTC), 1 January 1970.
    Timestamp(int64_t ms=0) : milliseconds(ms) {}
    operator pn_timestamp_t() const { return milliseconds; }
    bool operator==(const Timestamp& x) { return milliseconds == x.milliseconds; }
    bool operator<(const Timestamp& x) { return milliseconds < x.milliseconds; }
};

///@}

template <class T> struct TypeIdOf {};
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

template<class T, TypeId A> struct TypePair {
    typedef T CppType;
    TypeId type;
};

template<class T, TypeId A> struct Ref : public TypePair<T, A> {
    Ref(T& v) : value(v) {}
    T& value;
};

template<class T, TypeId A> struct CRef : public TypePair<T, A> {
    CRef(const T& v) : value(v) {}
    CRef(const Ref<T,A>& ref) : value(ref.value) {}
    const T& value;
};

/** Create a reference to value as AMQP type A for decoding. For example to decode an array of Int:
 *
 *     std::vector<Int> v;
 *     decoder >> as<ARRAY>(v);
 */
template <TypeId A, class T> Ref<T, A> as(T& value) { return Ref<T, A>(value); }

/** Create a const reference to value as AMQP type A for encoding. */
template <TypeId A, class T> CRef<T, A> as(const T& value) { return CRef<T, A>(value); }

///@}

// TODO aconway 2015-06-16: described types.

/** Return the name of a type. */
PN_CPP_EXTERN std::string typeName(TypeId);

/** Print the name of a type */
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, TypeId);

/** Return the name of a type from a class. */
PN_CPP_EXTERN template<class T> std::string typeName() { return typeName(TypeIdOf<T>::value); }

/** Information needed to start extracting or inserting a container type.
 *
 * With a decoder you can use `Start s = decoder.start()` or `Start s; decoder > s`
 * to get the Start for the current container.
 *
 * With an encoder use one of the member functions startArray, startList, startMap or startDescribed
 * to create an appropriate Start value, e.g. `encoder << startList() << ...`
 */
PN_CPP_EXTERN struct Start {
    Start(TypeId type=NULL_, TypeId element=NULL_, bool described=false, size_t size=0);
    TypeId type;            ///< The container type: ARRAY, LIST, MAP or DESCRIBED.
    TypeId element;         ///< the element type for array only.
    bool isDescribed;       ///< true if first value is a descriptor.
    size_t size;            ///< the element count excluding the descriptor (if any)

    /** Return a Start for an array */
    static Start array(TypeId element, bool described=false);
    /** Return a Start for a list */
    static Start list();
    /** Return a Start for a map */
    static Start map();
    /** Return a Start for a described type */
    static Start described();
};

/** Finish insterting or extracting a container value. */
PN_CPP_EXTERN struct Finish {};
inline Finish finish() { return Finish(); }

/** Skip a value */
PN_CPP_EXTERN struct Skip{};
inline Skip skip() { return Skip(); }

}

#endif // TYPES_H
