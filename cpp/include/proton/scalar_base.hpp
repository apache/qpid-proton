#ifndef PROTON_SCALAR_BASE_HPP
#define PROTON_SCALAR_BASE_HPP

/*
 *
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
 *
 */

#include "./binary.hpp"
#include "./decimal.hpp"
#include "./error.hpp"
#include "./internal/comparable.hpp"
#include "./internal/export.hpp"
#include "./internal/type_traits.hpp"
#include "./symbol.hpp"
#include "./timestamp.hpp"
#include "./type_id.hpp"
#include "./types_fwd.hpp"
#include "./uuid.hpp"

#include <proton/type_compat.h>

#include <iosfwd>
#include <string>
#include <typeinfo>

/// @file
/// @copybrief proton::scalar_base

namespace proton {

class scalar_base;

namespace codec {
class decoder;
class encoder;
}

namespace internal {
template<class T> T get(const scalar_base& s);
}

/// The base class for scalar types.
class scalar_base : private internal::comparable<scalar_base> {
  public:
    /// AMQP type of data stored in the scalar
    PN_CPP_EXTERN type_id type() const;

    /// True if there is no value, i.e. type() == NULL_TYPE.
    PN_CPP_EXTERN bool empty() const;

    /// Compare
  friend PN_CPP_EXTERN bool operator<(const scalar_base& x, const scalar_base& y);
    /// Compare
  friend PN_CPP_EXTERN bool operator==(const scalar_base& x, const scalar_base& y);
    /// Print the contained value
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, const scalar_base& x);

  protected:
    PN_CPP_EXTERN scalar_base(const pn_atom_t& a);
    PN_CPP_EXTERN scalar_base();
    PN_CPP_EXTERN scalar_base(const scalar_base&);
    PN_CPP_EXTERN scalar_base& operator=(const scalar_base&);

    PN_CPP_EXTERN void put_(bool);
    PN_CPP_EXTERN void put_(uint8_t);
    PN_CPP_EXTERN void put_(int8_t);
    PN_CPP_EXTERN void put_(uint16_t);
    PN_CPP_EXTERN void put_(int16_t);
    PN_CPP_EXTERN void put_(uint32_t);
    PN_CPP_EXTERN void put_(int32_t);
    PN_CPP_EXTERN void put_(uint64_t);
    PN_CPP_EXTERN void put_(int64_t);
    PN_CPP_EXTERN void put_(wchar_t);
    PN_CPP_EXTERN void put_(float);
    PN_CPP_EXTERN void put_(double);
    PN_CPP_EXTERN void put_(timestamp);
    PN_CPP_EXTERN void put_(const decimal32&);
    PN_CPP_EXTERN void put_(const decimal64&);
    PN_CPP_EXTERN void put_(const decimal128&);
    PN_CPP_EXTERN void put_(const uuid&);
    PN_CPP_EXTERN void put_(const std::string&);
    PN_CPP_EXTERN void put_(const symbol&);
    PN_CPP_EXTERN void put_(const binary&);
    PN_CPP_EXTERN void put_(const char* s); ///< Treated as an AMQP string
    PN_CPP_EXTERN void put_(const null&);
#if PN_CPP_HAS_NULLPTR
    PN_CPP_EXTERN void put_(decltype(nullptr));
#endif

    template<class T> void put(const T& x) { putter<T>::put(*this, x); }

  private:
    PN_CPP_EXTERN void get_(bool&) const;
    PN_CPP_EXTERN void get_(uint8_t&) const;
    PN_CPP_EXTERN void get_(int8_t&) const;
    PN_CPP_EXTERN void get_(uint16_t&) const;
    PN_CPP_EXTERN void get_(int16_t&) const;
    PN_CPP_EXTERN void get_(uint32_t&) const;
    PN_CPP_EXTERN void get_(int32_t&) const;
    PN_CPP_EXTERN void get_(uint64_t&) const;
    PN_CPP_EXTERN void get_(int64_t&) const;
    PN_CPP_EXTERN void get_(wchar_t&) const;
    PN_CPP_EXTERN void get_(float&) const;
    PN_CPP_EXTERN void get_(double&) const;
    PN_CPP_EXTERN void get_(timestamp&) const;
    PN_CPP_EXTERN void get_(decimal32&) const;
    PN_CPP_EXTERN void get_(decimal64&) const;
    PN_CPP_EXTERN void get_(decimal128&) const;
    PN_CPP_EXTERN void get_(uuid&) const;
    PN_CPP_EXTERN void get_(std::string&) const;
    PN_CPP_EXTERN void get_(symbol&) const;
    PN_CPP_EXTERN void get_(binary&) const;
    PN_CPP_EXTERN void get_(null&) const;
#if PN_CPP_HAS_NULLPTR
    PN_CPP_EXTERN void get_(decltype(nullptr)&) const;
#endif


    // use template structs, functions cannot be  partially specialized.
    template <class T, class Enable=void> struct putter {
        static void put(scalar_base& s, const T& x) { s.put_(x); }
    };
    template <class T>
    struct putter<T, typename internal::enable_if<internal::is_unknown_integer<T>::value>::type> {
        static void put(scalar_base& s, const T& x) {
            s.put_(static_cast<typename internal::known_integer<T>::type>(x));
        }
    };
    template <class T, class Enable=void>
    struct getter {
        static T get(const scalar_base& s) { T x; s.get_(x); return x; }
    };
    template <class T>
    struct getter<T, typename internal::enable_if<internal::is_unknown_integer<T>::value>::type> {
        static T get(const scalar_base& s) {
            typename internal::known_integer<T>::type x; s.get_(x); return x;
        }
    };

    void ok(pn_type_t) const;
    void set(const pn_atom_t&);
    void set(const binary& x, pn_type_t t);

    pn_atom_t atom_;
    binary bytes_; // Hold binary data.

    /// @cond INTERNAL
  friend class message;
  friend class codec::encoder;
  friend class codec::decoder;
  template<class T> friend T internal::get(const scalar_base& s);
    /// @endcond
};

namespace internal {

template<class T> T get(const scalar_base& s) {
      return scalar_base::getter<T>::get(s);
}

template <class R, class F> R visit(const scalar_base& s, F f) {
    switch(s.type()) {
      case BOOLEAN: return f(internal::get<bool>(s));
      case UBYTE: return f(internal::get<uint8_t>(s));
      case BYTE: return f(internal::get<int8_t>(s));
      case USHORT: return f(internal::get<uint16_t>(s));
      case SHORT: return f(internal::get<int16_t>(s));
      case UINT: return f(internal::get<uint32_t>(s));
      case INT: return f(internal::get<int32_t>(s));
      case CHAR: return f(internal::get<wchar_t>(s));
      case ULONG: return f(internal::get<uint64_t>(s));
      case LONG: return f(internal::get<int64_t>(s));
      case TIMESTAMP: return f(internal::get<timestamp>(s));
      case FLOAT: return f(internal::get<float>(s));
      case DOUBLE: return f(internal::get<double>(s));
      case DECIMAL32: return f(internal::get<decimal32>(s));
      case DECIMAL64: return f(internal::get<decimal64>(s));
      case DECIMAL128: return f(internal::get<decimal128>(s));
      case UUID: return f(internal::get<uuid>(s));
      case BINARY: return f(internal::get<binary>(s));
      case STRING: return f(internal::get<std::string>(s));
      case SYMBOL: return f(internal::get<symbol>(s));
      default: throw conversion_error("invalid scalar type "+type_name(s.type()));
    }
}

PN_CPP_EXTERN conversion_error make_coercion_error(const char* cpp_type, type_id amqp_type);

template<class T> struct coerce_op {
    template <class U>
    typename enable_if<is_convertible<U, T>::value, T>::type operator()(const U& x) {
        return static_cast<T>(x);
    }
    template <class U>
    typename enable_if<!is_convertible<U, T>::value, T>::type operator()(const U&) {
        throw make_coercion_error(typeid(T).name(), type_id_of<U>::value);
    }
};

template <class T> T coerce(const scalar_base& s) { return visit<T>(s, coerce_op<T>()); }
} // namespace internal

/// Return a readable string representation of x for display purposes.
PN_CPP_EXTERN std::string to_string(const scalar_base& x);

} // proton

#endif // PROTON_SCALAR_BASE_HPP
