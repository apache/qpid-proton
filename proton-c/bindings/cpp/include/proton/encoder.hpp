#ifndef ENCODER_H
#define ENCODER_H

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
/// XXX change namespace, review better

#include "proton/error.hpp"
#include "proton/types.hpp"
#include "proton/type_traits.hpp"
#include "proton/object.hpp"
#include <iosfwd>

#ifndef PN_NO_CONTAINER_CONVERT

#include <vector>
#include <deque>
#include <list>
#include <map>

#if PN_HAS_CPP11
#include <array>
#include <forward_list>
#include <unordered_map>
#endif // PN_HAS_CPP11

#endif // PN_NO_CONTAINER_CONVERT

/// @file
/// @internal

struct pn_data_t;

namespace proton {

class scalar;
class data;
class message_id;
class annotation_key;
class value;

template<class T, type_id A> struct cref {
    typedef T cpp_type;
    static const type_id type;

    cref(const T& v) : value(v) {}
    const T& value;
};

template <class T, type_id A> const type_id cref<T, A>::type = A;

/**
 * Indicate the desired AMQP type to use when encoding T.
 */
template <type_id A, class T> cref<T, A> as(const T& value) { return cref<T, A>(value); }

/// Stream-like encoder from AMQP bytes to C++ values.
///
/// Internal use only, see proton::value, proton::scalar and proton::amqp
/// for the recommended ways to manage AMQP data.
class encoder : public internal::object<pn_data_t> {
  public:
    encoder(pn_data_t* e) : internal::object<pn_data_t>(e) {}

    /**
     * Encode the current values into buffer and update size to reflect the number of bytes encoded.
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

    PN_CPP_EXTERN class data data();

    /** @name Insert simple types.
     *@{
     */
  friend PN_CPP_EXTERN encoder operator<<(encoder, bool);
  friend PN_CPP_EXTERN encoder operator<<(encoder, uint8_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, int8_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, uint16_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, int16_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, uint32_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, int32_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, wchar_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, uint64_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, int64_t);
  friend PN_CPP_EXTERN encoder operator<<(encoder, timestamp);
  friend PN_CPP_EXTERN encoder operator<<(encoder, float);
  friend PN_CPP_EXTERN encoder operator<<(encoder, double);
  friend PN_CPP_EXTERN encoder operator<<(encoder, decimal32);
  friend PN_CPP_EXTERN encoder operator<<(encoder, decimal64);
  friend PN_CPP_EXTERN encoder operator<<(encoder, decimal128);
  friend PN_CPP_EXTERN encoder operator<<(encoder, uuid);
  friend PN_CPP_EXTERN encoder operator<<(encoder, std::string);
  friend PN_CPP_EXTERN encoder operator<<(encoder, symbol);
  friend PN_CPP_EXTERN encoder operator<<(encoder, binary);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const message_id&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const annotation_key&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const value&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const scalar&);
    ///@}

    /**
     * Start a container type.
     */
  friend PN_CPP_EXTERN encoder operator<<(encoder, const start&);

    /** Finish a container type. See operator<<(encoder&, const start&) */
  friend PN_CPP_EXTERN encoder operator<<(encoder e, finish);


    /**@name Insert values returned by the as<type_id> helper.
     *@{
     */
  template <class T, type_id A> friend PN_CPP_EXTERN encoder operator<<(encoder, cref<T, A>);
  template <class T> friend encoder operator<<(encoder, cref<T, ARRAY>);
  template <class T> friend encoder operator<<(encoder, cref<T, LIST>);
  template <class T> friend encoder operator<<(encoder, cref<T, MAP>);
    // TODO aconway 2015-06-16: described values.
    ///@}
};

// Treat char* as string
inline encoder operator<<(encoder e, char* s) { return e << std::string(s); }
inline encoder operator<<(encoder e, const char* s) { return e << std::string(s); }

// operator << for integer types that are not covered by the standard overrides.
template <class T>
typename enable_if<is_unknown_integer<T>::value, encoder>::type operator<<(encoder e, T i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v = i;
    return e << v;              // Insert as a known integer type
}

// TODO aconway 2015-06-16: described array insertion.

template <class T> encoder operator<<(encoder e, cref<T, ARRAY> a) {
    e << start::array(type_id_of<typename T::value_type>::value);
    for (typename T::const_iterator i = a.value.begin(); i != a.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> encoder operator<<(encoder e, cref<T, LIST> l) {
    e << start::list();
    for (typename T::const_iterator i = l.value.begin(); i != l.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> encoder operator<<(encoder e, cref<T, MAP> m){
    e << start::map();
    for (typename T::const_iterator i = m.value.begin(); i != m.value.end(); ++i) {
        e << i->first;
        e << i->second;
    }
    e << finish();
    return e;
}

#ifndef PN_NO_CONTAINER_CONVERT
// Encode as ARRAY
template <class T, class A> encoder operator<<(encoder e, const std::vector<T, A>& v) { return e << as<ARRAY>(v); }
template <class T, class A> encoder operator<<(encoder e, const std::deque<T, A>& v) { return e << as<ARRAY>(v); }
template <class T, class A> encoder operator<<(encoder e, const std::list<T, A>& v) { return e << as<ARRAY>(v); }

// Encode as LIST
template <class A> encoder operator<<(encoder e, const std::vector<value, A>& v) { return e << as<LIST>(v); }
template <class A> encoder operator<<(encoder e, const std::deque<value, A>& v) { return e << as<LIST>(v); }
template <class A> encoder operator<<(encoder e, const std::list<value, A>& v) { return e << as<LIST>(v); }

// Encode as MAP
template <class K, class T, class C, class A> encoder operator<<(encoder e, const std::map<K, T, C, A>& v) { return e << as<MAP>(v); }

#if PN_HAS_CPP11

// Encode as ARRAY.
template <class T, class A> encoder operator<<(encoder e, const std::forward_list<T, A>& v) { return e << as<ARRAY>(v); }
template <class T, std::size_t N> encoder operator<<(encoder e, const std::array<T, N>& v) { return e << as<ARRAY>(v); }

// Encode as LIST.
template <class A> encoder operator<<(encoder e, const std::forward_list<value, A>& v) { return e << as<LIST>(v); }
template <std::size_t N> encoder operator<<(encoder e, const std::array<value, N>& v) { return e << as<LIST>(v); }

// Encode as map.
template <class K, class T, class C, class A> encoder operator<<(encoder e, const std::unordered_map<K, T, C, A>& v) { return e << as<MAP>(v); }

#endif // PN_HAS_CPP11

#endif // PN_NO_CONTAINER_CONVERT

}

/// @endcond

#endif // ENCODER_H
