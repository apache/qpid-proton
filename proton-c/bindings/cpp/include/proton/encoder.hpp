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
#include "proton/facade.hpp"
#include <iosfwd>

struct pn_data_t;

namespace proton {

class data;
class message_id;

/** Raised by encoder operations on error */
struct encode_error : public error { PN_CPP_EXTERN explicit encode_error(const std::string&) throw(); };

/**
 * Stream C++ data values into an AMQP encoder using operator<<.
 *
 * types.h defines C++ typedefs and types for AMQP each type. These types insert
 * as the corresponding AMQP type. Conversion rules apply to other types:
 *
 * - Integer types insert as the AMQP integer of matching size and signedness.
 * - std::string or char* insert as AMQP strings.
 *
 * C++ containers can be inserted as AMQP containers with the as() helper
 * functions. For example:
 *
 *     std::vector<amqp_symbol> v;
 *     encoder << as<amqp_list>(v);
 *
 * AMQP maps can be inserted from any container with std::pair<X,Y> as the
 * value_type. That includes std::map and std::unordered_map but also for
 * example std::vector<std::pair<X,Y> >. This allows you to control the order
 * of elements when inserting AMQP maps.
 *
 * You can also insert containers element-by-element, see operator<<(encoder&, const start&)
 *
 *@throw decoder::error if the curent value is not a container type.
 */
class encoder : public facade<pn_data_t, encoder> {
  public:
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

    PN_CPP_EXTERN class data& data();

    /** @name Insert simple types.
     *@{
     */
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_null);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_boolean);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_ubyte);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_byte);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_ushort);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_short);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_uint);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_int);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_char);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_ulong);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_long);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_timestamp);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_float);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_double);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_decimal32);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_decimal64);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_decimal128);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_uuid);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_string);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_symbol);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, amqp_binary);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, const message_id&);
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, const class data&);
    ///@}

    /**
     * Start a container type.
     *
     * Use one of the static functions start::array(), start::list(),
     * start::map() or start::described() to create an appropriate start value
     * and insert it into the encoder, followed by the contained elements.  For
     * example:
     *
     *      encoder << start::list() << amqp_int(1) << amqp_symbol("two") << 3.0 << finish();
     */
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, const start&);

    /** Finish a container type. See operator<<(encoder&, const start&) */
  friend PN_CPP_EXTERN encoder& operator<<(encoder& e, finish);


    /**@name Insert values returned by the as<type_id> helper.
     *@{
     */
  template <class T, type_id A> friend PN_CPP_EXTERN encoder& operator<<(encoder&, cref<T, A>);
  template <class T> friend encoder& operator<<(encoder&, cref<T, ARRAY>);
  template <class T> friend encoder& operator<<(encoder&, cref<T, LIST>);
  template <class T> friend encoder& operator<<(encoder&, cref<T, MAP>);
    // TODO aconway 2015-06-16: described values.
    ///@}

    /** Copy data from a raw pn_data_t */
  friend PN_CPP_EXTERN encoder& operator<<(encoder&, pn_data_t*);
};

// Need to disambiguate char* conversion to bool and std::string as amqp_string.
inline encoder& operator<<(encoder& e, char* s) { return e << amqp_string(s); }
inline encoder& operator<<(encoder& e, const char* s) { return e << amqp_string(s); }
inline encoder& operator<<(encoder& e, const std::string& s) { return e << amqp_string(s); }

// operator << for integer types that are not covered by the standard overrides.
template <class T>
typename enable_if<is_unknown_integer<T>::value, encoder&>::type operator<<(encoder& e, T i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v = i;
    return e << v;              // Insert as a known integer type
}

// TODO aconway 2015-06-16: described array insertion.

template <class T> encoder& operator<<(encoder& e, cref<T, ARRAY> a) {
    e << start::array(type_id_of<typename T::value_type>::value);
    for (typename T::const_iterator i = a.value.begin(); i != a.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> encoder& operator<<(encoder& e, cref<T, LIST> l) {
    e << start::list();
    for (typename T::const_iterator i = l.value.begin(); i != l.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> encoder& operator<<(encoder& e, cref<T, MAP> m){
    e << start::map();
    for (typename T::const_iterator i = m.value.begin(); i != m.value.end(); ++i) {
        e << i->first;
        e << i->second;
    }
    e << finish();
    return e;
}
///@cond INTERNAL Convert a ref to a cref.
template <class T, type_id A> encoder& operator<<(encoder& e, ref<T, A> ref) {
    return e << cref<T,A>(ref);
}
///@endcond

}
#endif // ENCODER_H
