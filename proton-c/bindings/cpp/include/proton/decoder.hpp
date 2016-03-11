#ifndef PROTON_DECODER_HPP
#define PROTON_DECODER_HPP
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

#include <proton/data.hpp>
#include <proton/type_traits.hpp>

#include <utility>

namespace proton {

class annotation_key;
class message_id;
class scalar;
class value;

namespace codec {

/// Stream-like decoder from AMQP bytes to C++ values.
///
/// Internal use only, see proton::value, proton::scalar and proton::amqp
/// for the recommended ways to manage AMQP data.
class decoder : public data {
  public:
    ///@internal
    explicit decoder(const data& d) : data(d) {}

    /// Attach decoder to a proton::value. The decoder is rewound to the start of the data.
    PN_CPP_EXTERN explicit decoder(codec::exact_cref<value>);

    /// Decode AMQP data from a buffer and add it to the end of the decoders stream. */
    PN_CPP_EXTERN void decode(const char* buffer, size_t size);

    /// Decode AMQP data from a std::string and add it to the end of the decoders stream. */
    PN_CPP_EXTERN void decode(const std::string&);

    /// Return true if there are more value to extract at the current level.
    PN_CPP_EXTERN bool more();

    /// Get the type of the next value that will be read by operator>>.
    /// @throw conversion_error if no more values. @see decoder::more().
    PN_CPP_EXTERN type_id next_type();

    /** @name Extract simple types
     * Overloads to extract simple types.
     * @throw conversion_error if the decoder is empty or has an incompatible type.
     * @{
     */
    PN_CPP_EXTERN decoder& operator>>(bool&);
    PN_CPP_EXTERN decoder& operator>>(uint8_t&);
    PN_CPP_EXTERN decoder& operator>>(int8_t&);
    PN_CPP_EXTERN decoder& operator>>(uint16_t&);
    PN_CPP_EXTERN decoder& operator>>(int16_t&);
    PN_CPP_EXTERN decoder& operator>>(uint32_t&);
    PN_CPP_EXTERN decoder& operator>>(int32_t&);
    PN_CPP_EXTERN decoder& operator>>(wchar_t&);
    PN_CPP_EXTERN decoder& operator>>(uint64_t&);
    PN_CPP_EXTERN decoder& operator>>(int64_t&);
    PN_CPP_EXTERN decoder& operator>>(timestamp&);
    PN_CPP_EXTERN decoder& operator>>(float&);
    PN_CPP_EXTERN decoder& operator>>(double&);
    PN_CPP_EXTERN decoder& operator>>(decimal32&);
    PN_CPP_EXTERN decoder& operator>>(decimal64&);
    PN_CPP_EXTERN decoder& operator>>(decimal128&);
    PN_CPP_EXTERN decoder& operator>>(uuid&);
    PN_CPP_EXTERN decoder& operator>>(std::string&);
    PN_CPP_EXTERN decoder& operator>>(symbol&);
    PN_CPP_EXTERN decoder& operator>>(binary&);
    PN_CPP_EXTERN decoder& operator>>(message_id&);
    PN_CPP_EXTERN decoder& operator>>(annotation_key&);
    PN_CPP_EXTERN decoder& operator>>(scalar&);
    PN_CPP_EXTERN decoder& operator>>(value&);
    PN_CPP_EXTERN decoder& operator>>(null&);
    ///@}

    /// Start decoding a container type, such as an ARRAY, LIST or MAP.
    /// This "enters" the container, more() will return false at the end of the container.
    /// Call finish() to "exit" the container and move on to the next value.
    PN_CPP_EXTERN decoder& operator>>(start&);

    // Finish decoding a container type, and move on to the next value in the stream.
    PN_CPP_EXTERN decoder& operator>>(const finish&);

    // XXX doc
    template <class T> struct sequence_ref { T& ref; sequence_ref(T& r) : ref(r) {} };
    template <class T> struct associative_ref { T& ref; associative_ref(T& r) : ref(r) {} };
    template <class T> struct pair_sequence_ref { T& ref;  pair_sequence_ref(T& r) : ref(r) {} };

    template <class T> static sequence_ref<T> sequence(T& x) { return sequence_ref<T>(x); }
    template <class T> static associative_ref<T> associative(T& x) { return associative_ref<T>(x); }
    template <class T> static pair_sequence_ref<T> pair_sequence(T& x) { return pair_sequence_ref<T>(x); }

    /** Extract any AMQP sequence (ARRAY, LIST or MAP) to a C++ sequence
     * container of T if the elements types are convertible to T. A MAP is
     * extracted as [key1, value1, key2, value2...]
     */
    template <class T> decoder& operator>>(sequence_ref<T> r)  {
        start s;
        *this >> s;
        if (s.is_described) next();
        r.ref.resize(s.size);
        for (typename T::iterator i = r.ref.begin(); i != r.ref.end(); ++i)
            *this >> *i;
        return *this;
    }

    /** Extract an AMQP MAP to a C++ associative container */
    template <class T> decoder& operator>>(associative_ref<T> r)  {
        start s;
        *this >> s;
        assert_type_equal(MAP, s.type);
        r.ref.clear();
        for (size_t i = 0; i < s.size/2; ++i) {
            typename remove_const<typename T::key_type>::type k;
            typename remove_const<typename T::mapped_type>::type v;
            *this >> k >> v;
            r.ref[k] = v;
        }
        return *this;
    }

    /// Extract an AMQP MAP to a C++ push_back sequence of pairs
    /// preserving encoded order.
    template <class T> decoder& operator>>(pair_sequence_ref<T> r)  {
        start s;
        *this >> s;
        assert_type_equal(MAP, s.type);
        r.ref.clear();
        for (size_t i = 0; i < s.size/2; ++i) {
            typedef typename T::value_type value_type;
            typename remove_const<typename value_type::first_type>::type k;
            typename remove_const<typename value_type::second_type>::type v;
            *this >> k >> v;
            r.ref.push_back(value_type(k, v));
        }
        return *this;
    }

  private:
    type_id pre_get();
    template <class T, class U> decoder& extract(T& x, U (*get)(pn_data_t*));

  friend class message;
};

// operator >> for integer types that are not covered by the standard overrides.
template <class T> typename codec::enable_unknown_integer<T, decoder&>::type
operator>>(decoder& d, T& i)  {
    typename integer_type<sizeof(T), is_signed<T>::value>::type v;
    d >> v;                     // Extract as a known integer type
    i = v;                      // C++ conversion to the target type.
    return d;
}

///@cond INTERNAL

} // internal
} // proton

/// @endcond

#endif // PROTON_DECODER_HPP
