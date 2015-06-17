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
#include "proton/types.hpp"
#include "proton/Error.hpp"
#include <iosfwd>

struct pn_data_t;

namespace proton {


class Value;

/**@file
 * Stream-like encoder from C++ values to AMQP bytes.
 * @ingroup cpp
*/

/** Raised by Encoder operations on error */
struct EncodeError : public Error { explicit EncodeError(const std::string&) throw(); };

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
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Null);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Bool);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ubyte);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Byte);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ushort);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Short);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Uint);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Int);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Char);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Ulong);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Long);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Timestamp);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Float);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Double);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal32);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal64);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Decimal128);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Uuid);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, String);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Symbol);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, Binary);
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, const Value&);
    ///@}

    /** Start a container type. See the Start class. */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, const Start&);

    /** Finish a container type. */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder& e, Finish);


    /**@name Insert values returned by the as<TypeId> helper.
     *@{
     */
  template <class T, TypeId A> friend Encoder& operator<<(Encoder&, CRef<T, A>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, ARRAY>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, LIST>);
  template <class T> friend Encoder& operator<<(Encoder&, CRef<T, MAP>);
    // TODO aconway 2015-06-16: DESCRIBED.
    ///@}

    /** Copy data from a raw pn_data_t */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, pn_data_t*);
  private:
    PN_CPP_EXTERN Encoder(pn_data_t* pd);

    // Not implemented
    Encoder(const Encoder&);
    Encoder& operator=(const Encoder&);

  friend class Value;
};

/** Encode const char* as string */
inline Encoder& operator<<(Encoder& e, const char* s) { return e << String(s); }

/** Encode char* as string */
inline Encoder& operator<<(Encoder& e, char* s) { return e << String(s); }

/** Encode std::string as string */
inline Encoder& operator<<(Encoder& e, const std::string& s) { return e << String(s); }

//@internal Convert a Ref to a CRef.
template <class T, TypeId A> Encoder& operator<<(Encoder& e, Ref<T, A> ref) {
    return e << CRef<T,A>(ref);
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


}
#endif // ENCODER_H
