#ifndef DECODER_H
#define DECODER_H
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

#include "proton/cpp/Data.h"
#include "proton/cpp/types.h"
#include "proton/cpp/exceptions.h"
#include <iosfwd>

namespace proton {
namespace reactor {

class Value;

/**@file
 * Stream-like decoder from AMQP bytes to C++ values.
 * @ingroup cpp
 */

/**
@ingroup cpp

Stream-like decoder from AMQP bytes to a stream of C++ values.

types.h defines C++ types corresponding to AMQP types.

Decoder operator>> will extract AMQP types into corresponding C++ types, and do
simple conversions, e.g. from AMQP integer types to corresponding or larger C++
integer types.

You can require an exact AMQP type using the `as<type>(value)` helper. E.g.

    Int i;
    decoder >> as<INT>(i):       // Will throw if decoder does not contain an INT

You can also use the `as` helper to extract an AMQP list, array or map into C++ containers.

    std::vector<Int> v;
    decoder >> as<LIST>(v);     // Extract a list of INT.

AMQP maps can be inserted/extracted to any container with pair<X,Y> as
value_type, which includes std::map and std::unordered_map but also for
example std::vector<std::pair<X,Y> >. This allows you to perserve order when
extracting AMQP maps.

You can also extract container values element-by-element, see the Start class.
*/
PN_CPP_EXTERN class Decoder : public virtual Data {
  public:
    /** Raised if a Decoder operation fails  */
    struct Error : public ProtonException {
        explicit Error(const std::string& msg) throw() : ProtonException(msg) {}
    };

    PN_CPP_EXTERN Decoder();
    PN_CPP_EXTERN ~Decoder();

    /** Copy AMQP data from a byte buffer into the Decoder. */
    PN_CPP_EXTERN Decoder(const char* buffer, size_t size);

    /** Copy AMQP data from a std::string into the Decoder. */
    PN_CPP_EXTERN Decoder(const std::string&);

    /** Decode AMQP data from a byte buffer onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const char* buffer, size_t size);

    /** Decode AMQP data from bytes in std::string onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const std::string&);

    /** Return true if there are more values to read at the current level. */
    PN_CPP_EXTERN bool more() const;

    /** Type of the next value that will be read by operator>>
     *@throw Error if empty().
     */
    PN_CPP_EXTERN TypeId type() const;

    /** @name Extract simple types
     * Overloads to extract simple types.
     * @throw Error if the Decoder is empty or the current value has an incompatible type.
     * @{
     */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Null);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Bool&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ubyte&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Byte&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ushort&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Short&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Uint&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Int&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Char&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Ulong&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Long&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Timestamp&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Float&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Double&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal32&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal64&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Decimal128&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Uuid&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, std::string&);
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Value&);
    ///@}

    /** Extract and return a value of type T. */
    template <class T> T get() { T value; *this >> value; return value; }

    /** Extract and return a value of type T, as AMQP type. */
    template <class T, TypeId A> T getAs() { T value; *this >> as<A>(value); return value; }

    /** Call Decoder::start() in constructor, Decoder::finish in destructor() */
    struct Scope : public Start {
        Decoder& decoder;
        Scope(Decoder& d) : decoder(d) { d >> *this; }
        ~Scope() { decoder >> finish(); }
    };

    template <TypeId A, class T> friend Decoder& operator>>(Decoder& d, Ref<T, A> ref) {
        d.checkType(A);
        d >> ref.value;
        return d;
    }

    /** start extracting a container value, one of array, list, map, described.
     * The basic pattern is:
     *
     *     Start s;
     *     decoder >> s;
     *     // check s.type() to see if this is an ARRAY, LIST, MAP or DESCRIBED type.
     *     if (s.described) extract the descriptor...
     *     for (size_t i = 0; i < s.size(); ++i) Extract each element...
     *     decoder >> finish();
     *
     * The first value of an ARRAY is a descriptor if Start::descriptor is true,
     * followed by Start::size elemets of type Start::element.
     *
     * A LIST has Start::size elements which may be of mixed type.
     *
     * A MAP has Start::size elements which alternate key, value, key, value...
     * and may be of mixed type.
     *
     * A DESCRIBED contains a descriptor and a single element, so it always has
     * Start::described=true and Start::size=1.
     *
     * Note Scope automatically calls finish() in its destructor.
     *
     *@throw decoder::error if the curent value is not a container type.
     */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Start&);

    /** Finish extracting a container value. */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Finish);

    /** Skip a value */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Skip);

  private:
    template <class T> Decoder& extract(T& value);
    void checkType(TypeId);

    // Not implemented
    Decoder(const Decoder&);
    Decoder& operator=(const Decoder&);

  friend class Value;
  friend class Encoder;
};

template <class T> Decoder& operator>>(Decoder& d, Ref<T, ARRAY> ref)  {
    Decoder::Scope s(d);
    if (s.isDescribed) d >> skip();
    ref.value.clear();
    ref.value.resize(s.size);
    for (typename T::iterator i = ref.value.begin(); i != ref.value.end(); ++i) {
        d >> *i;
    }
    return d;
}

template <class T> Decoder& operator>>(Decoder& d, Ref<T, LIST> ref)  {
    Decoder::Scope s(d);
    ref.value.clear();
    ref.value.resize(s.size);
    for (typename T::iterator i = ref.value.begin(); i != ref.value.end(); ++i)
        d >> *i;
    return d;
}

template <class T> Decoder& operator>>(Decoder& d, Ref<T, MAP> ref)  {
    Decoder::Scope m(d);
    ref.value.clear();
    for (size_t i = 0; i < m.size/2; ++i) {
        typename T::key_type k;
        typename T::mapped_type v;
        d >> k >> v;
        ref.value[k] = v;
    }
    return d;
}

}} // namespace proton::reactor
#endif // DECODER_H
