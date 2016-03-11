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

#include <vector>
#include <deque>
#include <list>
#include <map>

#if PN_CPP_HAS_CPP11
#include <array>
#include <forward_list>
#include <unordered_map>
#endif // PN_CPP_HAS_CPP11

/// @file
/// @internal

struct pn_data_t;

namespace proton {

class scalar;
class value;

namespace internal {

class data;

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
class encoder : public object<pn_data_t> {
  public:
    encoder(pn_data_t* e) : object<pn_data_t>(e) {}

    /**
     * Encode the current values into buffer and update size to reflect the
     * number of bytes encoded.
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

    PN_CPP_EXTERN encoder operator<<(bool);
    PN_CPP_EXTERN encoder operator<<(uint8_t);
    PN_CPP_EXTERN encoder operator<<(int8_t);
    PN_CPP_EXTERN encoder operator<<(uint16_t);
    PN_CPP_EXTERN encoder operator<<(int16_t);
    PN_CPP_EXTERN encoder operator<<(uint32_t);
    PN_CPP_EXTERN encoder operator<<(int32_t);
    PN_CPP_EXTERN encoder operator<<(wchar_t);
    PN_CPP_EXTERN encoder operator<<(uint64_t);
    PN_CPP_EXTERN encoder operator<<(int64_t);
    PN_CPP_EXTERN encoder operator<<(timestamp);
    PN_CPP_EXTERN encoder operator<<(float);
    PN_CPP_EXTERN encoder operator<<(double);
    PN_CPP_EXTERN encoder operator<<(decimal32);
    PN_CPP_EXTERN encoder operator<<(decimal64);
    PN_CPP_EXTERN encoder operator<<(decimal128);
    PN_CPP_EXTERN encoder operator<<(const uuid&);
    PN_CPP_EXTERN encoder operator<<(const std::string&);
    PN_CPP_EXTERN encoder operator<<(const symbol&);
    PN_CPP_EXTERN encoder operator<<(const binary&);
    PN_CPP_EXTERN encoder operator<<(const scalar&);
    PN_CPP_EXTERN encoder operator<<(const null&);

    /// Start encoding a complex type.
    PN_CPP_EXTERN encoder operator<<(const start&);
    /// Finish a complex type
    PN_CPP_EXTERN encoder operator<<(const finish&);

    // FIXME aconway 2016-03-03: hide
    PN_CPP_EXTERN void insert(const value& v);

  private:

  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const encoder&);

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


// FIXME aconway 2016-03-08: explain
template <class T> typename enable_if<is_same<T, value>::value, encoder>::type
operator<<(encoder e, const T& x) { e.insert(x); return e; }
    ///@}

// operator << for integer types that are not covered by the standard overrides.
template <class T> typename enable_integer<T, encoder>::type
operator<<(encoder e, T i)  {
    return e << static_cast<typename integer_type<sizeof(T), is_signed<T>::value>::type>(i);
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

#if PN_CPP_HAS_CPP11

// Encode as ARRAY.
template <class T, class A> encoder operator<<(encoder e, const std::forward_list<T, A>& v) { return e << as<ARRAY>(v); }
template <class T, std::size_t N> encoder operator<<(encoder e, const std::array<T, N>& v) { return e << as<ARRAY>(v); }

// Encode as LIST.
template <class A> encoder operator<<(encoder e, const std::forward_list<value, A>& v) { return e << as<LIST>(v); }
template <std::size_t N> encoder operator<<(encoder e, const std::array<value, N>& v) { return e << as<LIST>(v); }

// Encode as map.
template <class K, class T, class C, class A> encoder operator<<(encoder e, const std::unordered_map<K, T, C, A>& v) { return e << as<MAP>(v); }

#endif // PN_CPP_HAS_CPP11
}
}

/// @endcond

#endif // ENCODER_H
