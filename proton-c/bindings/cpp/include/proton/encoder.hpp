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

struct pn_data_t;

namespace proton {

class data;
class message_id;

/** Raised by encoder operations on error */
struct encode_error : public error { PN_CPP_EXTERN explicit encode_error(const std::string&) throw(); };

/**
 * Stream-like encoder from C++ values to AMQP values.
 *
 * types.hpp defines a C++ type for each AMQP type. For simple types they are
 * just typedefs for corresponding native C++ types. These types encode as the
 * corresponding AMQP type.
 *
 * There are some special case conversions:
 *
 * - Integer types other than those mentioned in types.hpp encode as the AMQP
 *   integer type of matching size and signedness.
 * - std::string or char* insert as AMQP STRING.
 *
 * For example to encode an AMQP INT, BOOLEAN and STRING these are equivalent:
 *
 *     enc << proton::amqp_int(1) << proton::amqp_boolean(true) << proton::amqp_string("foo");
 *     enc << int32_t(1) << true << "foo";
 *
 * You can force the encoding using the `proton::as` template function, for example:
 *
 *     uint64_t i = 100;
 *     enc << as<proton::SHORT>(i);
 *
 * C++ standard containers can be inserted. By default:
 *
 * - std::map and std::unordered_map encode as AMQP MAP
 * - std::vector, std::deque, std::list, std::array or std::forward_list encode as an AMQP ARRAY.
 * - std::vector<proton::value> etc. encode as AMQP LIST
 *
 * Again you can force the encoding using proton::as<LIST>() or proton::as<ARRAY>()
 *
 * Note that you can encode a sequence of pairs as a map, which allows you to control the
 * encoded order if that is important:
 *
 *     std::vector<std::pair<T1, T2> > v;
 *     enc << proton::as<MAP>(v);
 *
 * You can also insert containers element-by-element, see operator<<(encoder&, const start&)
 */
class encoder : public object<pn_data_t> {
  public:
    encoder(pn_data_t* e) : object(e) {}

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
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_null);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_boolean);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_ubyte);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_byte);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_ushort);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_short);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_uint);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_int);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_char);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_ulong);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_long);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_timestamp);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_float);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_double);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_decimal32);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_decimal64);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_decimal128);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_uuid);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_string);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_symbol);
  friend PN_CPP_EXTERN encoder operator<<(encoder, amqp_binary);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const message_id&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const value&);
    ///@}

    /**
     * Start a container type.
     *
     * Use one of the static functions start::array(), start::list(),
     * start::map() or start::described() to create an appropriate start value
     * and insert it into the encoder, followed by the contained elements.  For
     * example:
     *
     *      enc << start::list() << amqp_int(1) << amqp_symbol("two") << 3.0 << finish();
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

// Need to disambiguate char* conversion to bool and std::string as amqp_string.
inline encoder operator<<(encoder e, char* s) { return e << amqp_string(s); }
inline encoder operator<<(encoder e, const char* s) { return e << amqp_string(s); }
inline encoder operator<<(encoder e, const std::string& s) { return e << amqp_string(s); }

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
#endif // ENCODER_H
