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

/** Raised by decoder operations on error.*/
struct decode_error : public error { PN_CPP_EXTERN explicit decode_error(const std::string&) throw(); };

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

/**
Stream-like decoder from AMQP bytes to C++ values.

@see types.hpp defines C++ types corresponding to AMQP types.

The decoder operator>> will extract AMQP types into any compatible C++
type or throw an exception if the types are not compatible.

+-------------------------+-------------------------------+
|AMQP type                |Compatible C++ types           |
+=========================+===============================+
|BOOLEAN                  |amqp_boolean, bool             |
+-------------------------+-------------------------------+
|signed integer type I    |C++ signed integer type T where|
|                         |sizeof(T) >= sizeof(I)         |
+-------------------------+-------------------------------+
|unsigned integer type U  |C++ unsigned integer type T    |
|                         |where sizeof(T) >= sizeof(U)   |
+-------------------------+-------------------------------+
|CHAR                     |amqp_char, wchar_t             |
+-------------------------+-------------------------------+
|FLOAT                    |amqp_float, float              |
+-------------------------+-------------------------------+
|DOUBLE                   |amqp_double, double            |
+-------------------------+-------------------------------+
|STRING                   |amqp_string, std::string       |
+-------------------------+-------------------------------+
|SYMBOL                   |amqp_symbol, std::string       |
+-------------------------+-------------------------------+
|BINARY                   |amqp_binary, std::string       |
+-------------------------+-------------------------------+
|DECIMAL<n>               |amqp_decimal<n>                |
+-------------------------+-------------------------------+
|TIMESTAMP                |amqp_timestamp                 |
+-------------------------+-------------------------------+
|UUID                     |amqp_uuid                      |
+-------------------------+-------------------------------+

The special proton::value type can hold any AMQP type, simple or compound.

By default operator >> will do any conversion that does not lose data. For example
any AMQP signed integer type can be extracted as follows:

    int64_t i;
    dec >> i;

You can assert the exact AMQP type with proton::assert_type, for example
the following will throw if the AMQP type is not an AMQP INT (32 bits)

    amqp_int i;
    dec >> assert_type(INT) >> i;       // Will throw if decoder does not contain an INT

You can extract AMQP ARRAY, LIST or MAP into standard C++ containers of compatible types, for example:

    std::vector<int32_t> v;
    dec >> v;

This will work if the decoder contains an AMQP ARRAY or LIST of SHORT or INT values. It won't work
for LONG or other types. It will also work for a MAP with keys and values that are SHORT OR INT,
the map will be "flattened" into a sequence [ key1, value1, key2, value2 ] This will work with
std::dequeue, std::array, std::list or std::forward_list.

You can extract a MAP into a std::map or std::unordered_map

    std::map<std::string, std::string> v;
    dec >> v;

This will work for any AMQP map with keys and values that are STRING, SYMBOL or BINARY.

If you have non-standard container types that meet the most basic requirements for
the container or associative-container concepts, you can use them via helper functions:

    my_sequence_type<int64_t> s;
    dec >> proton::to_sequence(s); // Decode sequence of integers
    my_map_type<amqp_string, bool> s;
    dec >> proton::to_map(s); // Decode map of string: bool.

Finally you can extract an AMQP LIST with mixed type elements into a container of proton::value, e.g.

    std::vector<proton::value> v;
    dec >> v;

You can also extract container values element-by-element, see decoder::operator>>(decoder&, start&)
*/
class decoder : public object<pn_data_t> {
  public:
    decoder(pn_data_t* d) : object(d) {}

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

    PN_CPP_EXTERN class data data();

    /** @name Extract simple types
     * Overloads to extract simple types.
     * @throw error if the decoder is empty or the current value has an incompatible type.
     * @{
     */
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_null);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_boolean&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_ubyte&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_byte&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_ushort&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_short&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_uint&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_int&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_char&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_ulong&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_long&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_timestamp&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_float&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_double&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_decimal32&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_decimal64&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_decimal128&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, amqp_uuid&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, std::string&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, message_id&);
    PN_CPP_EXTERN friend decoder operator>>(decoder, value&);
    ///@}

    /** Extract and return a value of type T. */
    template <class T> T extract() { T value; *this >> value; return value; }

    /** start extracting a container value, one of array, list, map, described.
     * The basic pattern is:
     *
     *     start s;
     *     dec >> s;
     *     // check s.type() to see if this is an ARRAY, LIST, MAP or DESCRIBED type.
     *     if (s.described) extract the descriptor...
     *     for (size_t i = 0; i < s.size(); ++i) Extract each element...
     *     dec >> finish();
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
    PN_CPP_EXTERN friend decoder operator>>(decoder, start&);

    /** Finish extracting a container value. */
    PN_CPP_EXTERN friend decoder operator>>(decoder, finish);

    /** Skip a value */
    PN_CPP_EXTERN friend decoder operator>>(decoder, struct skip);

    /** Throw an exception if decoder.type() != assert_type.type */
    PN_CPP_EXTERN friend decoder operator>>(decoder, assert_type);

    /** Rewind to the beginning */
    PN_CPP_EXTERN friend decoder operator>>(decoder, struct rewind);

  private:
    PN_CPP_EXTERN void check_type(type_id);

  friend class encoder;
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
template <class T>
typename enable_if<is_unknown_integer<T>::value, decoder>::type
operator>>(decoder d, T& i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v;
    d >> v;                     // Extract as a known integer type
    i = v;
    return d;
}

///@cond INTERNAL
template <class T> struct sequence_ref {
    sequence_ref(T& v) : value(v) {}
    T& value;
};

template <class T> struct map_ref {
    map_ref(T& v) : value(v) {}
    T& value;
};

template <class T> struct pairs_ref {
    pairs_ref(T& v) : value(v) {}
    T& value;
};
///@endcond

/**
 * Return a wrapper for a C++ container to be decoded as a sequence. The AMQP
 * ARRAY, LIST, and MAP types can all be decoded as a sequence, a map will be
 * decoded as alternating key and value (provided they can both be converted to
 * the container's value_type)
 *
 * The following expressions must be valid for T t;
 *     T::iterator
 *     t.clear()
 *     t.resize()
 *     t.begin()
 *     t.end()
 */
template <class T> sequence_ref<T> to_sequence(T& v) { return sequence_ref<T>(v); }

/** Return a wrapper for a C++ map container to be decoded from an AMQP MAP.
 * The following expressions must be valid for T t;
 *     T::key_type
 *     T::mapped_type
 *     t.clear()
 *     T::key_type k; T::mapped_type v; t[k] = v;
 */
template <class T> map_ref<T> to_map(T& v) { return map_ref<T>(v); }

/** Return a wrapper for a C++ container of std::pair that can be decoded from AMQP maps,
 * preserving the encoded map order.
 *
 * The following expressions must be valid for T t;
 *     T::iterator
 *     t.clear()
 *     t.resize()
 *     t.begin()
 *     t.end()
 *     T::iterator i; i->first; i->second
 */
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

#ifndef PN_NO_CONTAINER_CONVERT

// Decode to sequence.
template <class T, class A> decoder operator>>(decoder d, std::vector<T, A>& v) { return d >> to_sequence(v); }
template <class T, class A> decoder operator>>(decoder d, std::deque<T, A>& v) { return d >> to_sequence(v); }
template <class T, class A> decoder operator>>(decoder d, std::list<T, A>& v) { return d >> to_sequence(v); }

// Decode to map.
template <class K, class T, class C, class A> decoder operator>>(decoder d, std::map<K, T, C, A>& v) { return d >> to_map(v); }

#if PN_HAS_CPP11

// Decode to sequence.
template <class T, class A> decoder operator>>(decoder d, std::forward_list<T, A>& v) { return d >> to_sequence(v); }
template <class T, std::size_t N> decoder operator>>(decoder d, std::array<T, N>& v) { return d >> to_sequence(v); }

// Decode to map.
template <class K, class T, class C, class A> decoder operator>>(decoder d, std::unordered_map<K, T, C, A>& v) { return d >> to_map(v); }

#endif // PN_HAS_CPP11
#endif // PN_NO_CONTAINER_CONVERT

}
#endif // DECODER_H
