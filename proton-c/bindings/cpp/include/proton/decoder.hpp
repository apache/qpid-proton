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

/// @cond INTERNAL
/// XXX change namespace, review better

#include "proton/error.hpp"
#include "proton/type_traits.hpp"
#include "proton/types.hpp"
#include "proton/object.hpp"
#include "proton/timestamp.hpp"
#include "proton/uuid.hpp"
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

struct pn_data_t;

namespace proton {

class scalar;
class message_id;
class annotation_key;
class value;

namespace internal {
class data;

/** Skips a value with `dec >> skip()`. */
struct skip{};

/** Assert the next type of value in the decoder: `dec >> assert_type(t)`
 *  throws if decoder.type() != t
 */
struct assert_type {
    type_id type;
    assert_type(type_id t) : type(t) {}
};

/** Rewind the decoder with `dec >> rewind()`. */
struct rewind{};

/// Stream-like decoder from AMQP bytes to C++ values.
///
/// Internal use only, see proton::value, proton::scalar and proton::amqp
/// for the recommended ways to manage AMQP data.
class decoder : public object<pn_data_t> {
  public:
    decoder(pn_data_t* d) : object<pn_data_t>(d) {}

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

    /** Skip one value */
    PN_CPP_EXTERN void skip();

    /** Back up by one value */
    PN_CPP_EXTERN void backup();

    /** @name Extract simple types
     * Overloads to extract simple types.
     * @throw error if the decoder is empty or the current value has an incompatible type.
     * @{
     */
    PN_CPP_EXTERN decoder operator>>(bool&);
    PN_CPP_EXTERN decoder operator>>(uint8_t&);
    PN_CPP_EXTERN decoder operator>>(int8_t&);
    PN_CPP_EXTERN decoder operator>>(uint16_t&);
    PN_CPP_EXTERN decoder operator>>(int16_t&);
    PN_CPP_EXTERN decoder operator>>(uint32_t&);
    PN_CPP_EXTERN decoder operator>>(int32_t&);
    PN_CPP_EXTERN decoder operator>>(wchar_t&);
    PN_CPP_EXTERN decoder operator>>(uint64_t&);
    PN_CPP_EXTERN decoder operator>>(int64_t&);
    PN_CPP_EXTERN decoder operator>>(timestamp&);
    PN_CPP_EXTERN decoder operator>>(float&);
    PN_CPP_EXTERN decoder operator>>(double&);
    PN_CPP_EXTERN decoder operator>>(decimal32&);
    PN_CPP_EXTERN decoder operator>>(decimal64&);
    PN_CPP_EXTERN decoder operator>>(decimal128&);
    PN_CPP_EXTERN decoder operator>>(uuid&);
    PN_CPP_EXTERN decoder operator>>(std::string&);
    PN_CPP_EXTERN decoder operator>>(message_id&);
    PN_CPP_EXTERN decoder operator>>(annotation_key&);
    PN_CPP_EXTERN decoder operator>>(scalar&);
    PN_CPP_EXTERN decoder operator>>(value&);
    ///@}

    // start extracting complex type.
    PN_CPP_EXTERN decoder operator>>(start&);

    /// Finish extracting a container value. */
    PN_CPP_EXTERN decoder operator>>(finish);

    /** Skip a value */
    PN_CPP_EXTERN decoder operator>>(struct skip);

    /** Throw an exception if decoder.type() != assert_type.type */
    PN_CPP_EXTERN decoder operator>>(assert_type);

    /** Rewind to the beginning */
    PN_CPP_EXTERN decoder operator>>(struct rewind);

  private:
    PN_CPP_EXTERN void check_type(type_id);
};

/** Call decoder::start() in constructor, decoder::finish in destructor().
 *
 */
struct scope : public start {
    decoder decoder_;
    scope(decoder d) : decoder_(d) { decoder_ >> *this; }
    ~scope() { decoder_ >> finish(); }
};

// operator >> for integer types that are not covered by the standard overrides.
template <class T> typename enable_integer<T, decoder>::type
operator>>(decoder d, T& i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v;
    d >> v;                     // Extract as a known integer type
    i = v;                      // C++ conversion to the target type.
    return d;
}

///@cond INTERNAL
template <class T> struct ref {
    ref(T& v) : value(v) {}
    T& value;
};
template <class T> struct sequence_ref : public ref<T> { sequence_ref(T& v) : ref<T>(v) {} };
template <class T> struct map_ref : public ref<T> { map_ref(T& v) : ref<T>(v) {} };
template <class T> struct pairs_ref : public ref<T> { pairs_ref(T& v) : ref<T>(v) {} };

// FIXME aconway 2016-03-03: rename

template <class T> sequence_ref<T> to_sequence(T& v) { return sequence_ref<T>(v); }

template <class T> map_ref<T> to_map(T& v) { return map_ref<T>(v); }

template <class T> pairs_ref<T> to_pairs(T& v) { return pairs_ref<T>(v); }

/** Extract any AMQP sequence (ARRAY, LIST or MAP) to a C++ container of T if
 * the elements types are convertible to T. A MAP is extracted as [key1, value1,
 * key2, value2...]
 */
template <class T> decoder operator>>(decoder d, sequence_ref<T> ref)  {
    scope s(d);
    if (s.is_described) d >> skip();
    T& v = ref.value;
    v.clear();
    v.resize(s.size);
    for (typename T::iterator i = v.begin(); i != v.end(); ++i)
        d >> *i;
    return d;
}

PN_CPP_EXTERN void assert_map_scope(const scope& s);

/** Extract an AMQP MAP to a C++ map */
template <class T> decoder operator>>(decoder d, map_ref<T> ref)  {
    scope s(d);
    assert_map_scope(s);
    T& m = ref.value;
    m.clear();
    for (size_t i = 0; i < s.size/2; ++i) {
        typename remove_const<typename T::key_type>::type k;
        typename remove_const<typename T::mapped_type>::type v;
        d >> k >> v;
        m[k] = v;
    }
    return d;
}

/** Extract an AMQP MAP to a C++ container of std::pair, preserving order. */
template <class T> decoder operator>>(decoder d, pairs_ref<T> ref)  {
    scope s(d);
    assert_map_scope(s);
    T& m = ref.value;
    m.clear();
    m.resize(s.size/2);
    for (typename T::iterator i = m.begin(); i != m.end(); ++i) {
        d >> i->first >> i->second;
    }
    return d;
}

// Decode to sequence.
template <class T, class A> decoder operator>>(decoder d, std::vector<T, A>& v) { return d >> to_sequence(v); }
template <class T, class A> decoder operator>>(decoder d, std::deque<T, A>& v) { return d >> to_sequence(v); }
template <class T, class A> decoder operator>>(decoder d, std::list<T, A>& v) { return d >> to_sequence(v); }

// Decode to map.
template <class K, class T, class C, class A> decoder operator>>(decoder d, std::map<K, T, C, A>& v) { return d >> to_map(v); }

#if PN_CPP_HAS_CPP11

// Decode to sequence.
template <class T, class A> decoder operator>>(decoder d, std::forward_list<T, A>& v) { return d >> to_sequence(v); }
template <class T, std::size_t N> decoder operator>>(decoder d, std::array<T, N>& v) { return d >> to_sequence(v); }

// Decode to map.
template <class K, class T, class C, class A> decoder operator>>(decoder d, std::unordered_map<K, T, C, A>& v) { return d >> to_map(v); }

#endif // PN_CPP_HAS_CPP11
}
}

/// @endcond

#endif // DECODER_H
