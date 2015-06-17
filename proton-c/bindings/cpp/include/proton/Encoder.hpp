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

#include "proton/Data.hpp"
#include "proton/Error.hpp"
#include "proton/types.hpp"
#include "proton/type_traits.hpp"
#include <iosfwd>

#include <iostream>             // FIXME aconway 2015-06-18:

struct pn_data_t;

namespace proton {

class Value;

/**@file
 * Stream-like encoder from C++ values to AMQP bytes.
 * @ingroup cpp
*/

/** Raised by Encoder operations on error */
struct EncodeError : public Error { PN_CPP_EXTERN explicit EncodeError(const std::string&) throw(); };

/**
@ingroup cpp

types.h defines C++ typedefs and types for AMQP each type. These types
insert as the corresponding AMQP type. Normal C++ conversion rules apply if you
insert any other type.

C++ containers can be inserted as AMQP containers with the as() helper functions. E.g.

   std::vector<Symbol> v; encoder << as<List>(v);

AMQP maps can be inserted/extracted to any container with pair<X,Y> as
value_type, which includes std::map and std::unordered_map but also for
example std::vector<std::pair<X,Y> >. This allows you to perserve order when
extracting AMQP maps.

You can also insert containers element-by-element, see the Start class.
*/
class Encoder : public virtual Data {
  public:
    PN_CPP_EXTERN Encoder();
    PN_CPP_EXTERN ~Encoder();

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

    /** @name Insert simple types.
     *@{
     */
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Null);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Bool);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Ubyte);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Byte);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Ushort);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Short);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Uint);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Int);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Char);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Ulong);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Long);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Timestamp);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Float);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Double);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Decimal32);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Decimal64);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Decimal128);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Uuid);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, String);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Symbol);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, Binary);
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, const Value&);
    ///@}

    /** Start a container type. See the Start class. */
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, const Start&);

    /** Finish a container type. */
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder& e, Finish);


    /**@name Insert values returned by the as<TypeId> helper.
     *@{
     */
  template <class T, TypeId A> friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, CRef<T, A>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, ARRAY>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, LIST>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, MAP>);
    // TODO aconway 2015-06-16: DESCRIBED.
    ///@}

    /** Copy data from a raw pn_data_t */
    friend PN_CPP_EXTERN Encoder& operator<<(Encoder&, pn_data_t*);

  private:
    PN_CPP_EXTERN Encoder(pn_data_t* pd);

  friend class Value;
};

// Need to disambiguate char* conversion to bool and std::string as String.
inline Encoder& operator<<(Encoder& e, char* s) { return e << String(s); }
inline Encoder& operator<<(Encoder& e, const char* s) { return e << String(s); }
inline Encoder& operator<<(Encoder& e, const std::string& s) { return e << String(s); }

// operator << for integer types that are not covered by the standard overrides.
template <class T>
typename std::enable_if<IsUnknownInteger<T>::value, Encoder&>::type operator<<(Encoder& e, T i)  {
    typename IntegerType<sizeof(T), std::is_signed<T>::value>::type v = i;
    return e << v;              // Insert as a known integer type
}

// TODO aconway 2015-06-16: described array insertion.

template <class T> Encoder& operator<<(Encoder& e, CRef<T, ARRAY> a) {
    e << Start::array(TypeIdOf<typename T::value_type>::value);
    for (typename T::const_iterator i = a.value.begin(); i != a.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> Encoder& operator<<(Encoder& e, CRef<T, LIST> l) {
    e << Start::list();
    for (typename T::const_iterator i = l.value.begin(); i != l.value.end(); ++i)
        e << *i;
    e << finish();
    return e;
}

template <class T> Encoder& operator<<(Encoder& e, CRef<T, MAP> m){
    e << Start::map();
    for (typename T::const_iterator i = m.value.begin(); i != m.value.end(); ++i) {
        e << i->first;
        e << i->second;
    }
    e << finish();
    return e;
}
//@internal Convert a Ref to a CRef.
template <class T, TypeId A> Encoder& operator<<(Encoder& e, Ref<T, A> ref) {
    return e << CRef<T,A>(ref);
}


}
#endif // ENCODER_H
