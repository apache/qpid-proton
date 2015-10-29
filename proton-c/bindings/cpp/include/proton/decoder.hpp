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

#include "proton/error.hpp"
#include "proton/type_traits.hpp"
#include "proton/types.hpp"
#include "proton/facade.hpp"
#include <iosfwd>

struct pn_data_t;

namespace proton {

class data;
class message_id;

/** Raised by decoder operations on error.*/
struct decode_error : public error { PN_CPP_EXTERN explicit decode_error(const std::string&) throw(); };

/** Skips a value with `decoder >> skip()`. */
struct skip{};

/** Rewind the decoder with `decoder >> rewind()`. */
struct rewind{};

/**
 * Stream-like decoder from AMQP bytes to a stream of C++ values.
 *
 * types.h defines C++ types corresponding to AMQP types.
 *
 * decoder operator>> will extract AMQP types into corresponding C++ types, and
 * do simple conversions, e.g. from AMQP integer types to corresponding or
 * larger C++ integer types.
 *
 * You can require an exact AMQP type using the `as<type>(value)` helper. E.g.
 *
 *     amqp_int i;
 *     decoder >> as<INT>(i):       // Will throw if decoder does not contain an INT
 *
 * You can also use the `as` helper to extract an AMQP list, array or map into C++ containers.
 *
 *
 *     std::vector<amqp_int> v;
 *     decoder >> as<LIST>(v);     // Extract a list of INT.
 *
 * AMQP maps can be inserted/extracted to any container with pair<X,Y> as
 * value_type, which includes std::map and std::unordered_map but also for
 * example std::vector<std::pair<X,Y> >. This allows you to preserve order when
 * extracting AMQP maps.
 *
 * You can also extract container values element-by-element, see decoder::operator>>(decoder&, start&)
 *
*/
class decoder : public facade<pn_data_t, decoder> {
  public:
    /** Copy AMQP data from a byte buffer into the decoder. */
    PN_CPP_EXTERN decoder(const char* buffer, size_t size);

    /** Copy AMQP data from a std::string into the decoder. */
    PN_CPP_EXTERN decoder(const std::string&);

    /** Decode AMQP data from a byte buffer onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const char* buffer, size_t size);

    /** Decode AMQP data from bytes in std::string onto the end of the value stream. */
    PN_CPP_EXTERN void decode(const std::string&);

    /** Return true if there are more values to read at the current level. */
    PN_CPP_EXTERN bool more() const;

    /** Type of the next value that will be read by operator>>
     *@throw error if empty().
     */
    PN_CPP_EXTERN type_id type() const;

    /** Rewind to the start of the data. */
    PN_CPP_EXTERN void rewind();

    /** Back up by one value */
    PN_CPP_EXTERN void backup();

    PN_CPP_EXTERN class data& data();

    /** @name Extract simple types
     * Overloads to extract simple types.
     * @throw error if the decoder is empty or the current value has an incompatible type.
     * @{
     */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_null);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_boolean&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_ubyte&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_byte&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_ushort&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_short&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_uint&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_int&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_char&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_ulong&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_long&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_timestamp&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_float&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_double&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_decimal32&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_decimal64&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_decimal128&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, amqp_uuid&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, std::string&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, message_id&);
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, class data&);
    ///@}

    /** Extract and return a value of type T. */
    template <class T> T get() { T value; *this >> value; return value; }

    /** Extract and return a value of type T, as AMQP type. */
    template <class T, type_id A> T get_as() { T value; *this >> as<A>(value); return value; }

    /** Call decoder::start() in constructor, decoder::finish in destructor().
     *
     */
    struct scope : public start {
        decoder& decoder_;
        scope(decoder& d) : decoder_(d) { d >> *this; }
        ~scope() { decoder_ >> finish(); }
    };

    template <type_id A, class T> friend decoder& operator>>(decoder& d, ref<T, A> ref) {
        d.check_type(A);
        d >> ref.value;
        return d;
    }

    /** start extracting a container value, one of array, list, map, described.
     * The basic pattern is:
     *
     *     start s;
     *     decoder >> s;
     *     // check s.type() to see if this is an ARRAY, LIST, MAP or DESCRIBED type.
     *     if (s.described) extract the descriptor...
     *     for (size_t i = 0; i < s.size(); ++i) Extract each element...
     *     decoder >> finish();
     *
     * The first value of an ARRAY is a descriptor if start::descriptor is true,
     * followed by start.size elements of type start::element.
     *
     * A LIST has start.size elements which may be of mixed type.
     *
     * A MAP has start.size elements which alternate key, value, key, value...
     * and may be of mixed type.
     *
     * A DESCRIBED contains a descriptor and a single element, so it always has
     * start.described=true and start.size=1.
     *
     * You must always end a complex type by extracting to an instance of `finish`,
     * the decoder::scope automates this.
     *
     *@throw decoder::error if the current value is not a container type.
     */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, start&);

    /** Finish extracting a container value. */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, finish);

    /** Skip a value */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, skip);

    /** Rewind to the beginning */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, struct rewind);

  private:
    PN_CPP_EXTERN void check_type(type_id);

  friend class encoder;
};

// operator >> for integer types that are not covered by the standard overrides.
template <class T>
typename enable_if<is_unknown_integer<T>::value, decoder&>::type operator>>(decoder& d, T& i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v;
    d >> v;                     // Extract as a known integer type
    i = v;
    return d;
}

template <class T> decoder& operator>>(decoder& d, ref<T, ARRAY> ref)  {
    decoder::scope s(d);
    if (s.is_described) d >> skip();
    ref.value.clear();
    ref.value.resize(s.size);
    for (typename T::iterator i = ref.value.begin(); i != ref.value.end(); ++i) {
        d >> *i;
    }
    return d;
}

template <class T> decoder& operator>>(decoder& d, ref<T, LIST> ref)  {
    decoder::scope s(d);
    ref.value.clear();
    ref.value.resize(s.size);
    for (typename T::iterator i = ref.value.begin(); i != ref.value.end(); ++i)
        d >> *i;
    return d;
}

template <class T> decoder& operator>>(decoder& d, ref<T, MAP> ref)  {
    decoder::scope m(d);
    ref.value.clear();
    for (size_t i = 0; i < m.size/2; ++i) {
        typename T::key_type k;
        typename T::mapped_type v;
        d >> k >> v;
        ref.value[k] = v;
    }
    return d;
}

}
#endif // DECODER_H
