#ifndef PROTON_INTERNAL_SCALAR_BASE_HPP
#define PROTON_INTERNAL_SCALAR_BASE_HPP

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

#include "../binary.hpp"
#include "../decimal.hpp"
#include "../error.hpp"
#include "./export.hpp"
#include "./comparable.hpp"
#include "./type_traits.hpp"
#include "../symbol.hpp"
#include "../timestamp.hpp"
#include "../type_id.hpp"
#include "../types_fwd.hpp"
#include "../uuid.hpp"

#include <iosfwd>
#include <string>
#include <typeinfo>

namespace proton {
class message;

namespace codec {
class decoder;
class encoder;
}

namespace internal {

class scalar_base;
template<class T> T get(const scalar_base& s);

/// Base class for scalar types.
class scalar_base : private comparable<scalar_base> {
  public:
    /// AMQP type of data stored in the scalar
    PN_CPP_EXTERN type_id type() const;

    // XXX I don't think many folks ever used this stuff.  Let's
    // remove it. - Yes, try to remove them.
    /// @cond INTERNAL
    /// deprecated
    template <class T> void get(T& x) const { get_(x); }
    template <class T> T get() const { T x; get_(x); return x; }
    /// @endcond

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

    // use template structs, functions cannot be  partially specialized.
    template <class T, class Enable=void> struct putter {
        static void put(scalar_base& s, const T& x) { s.put_(x); }
    };
    template <class T> struct putter<T, typename enable_if<is_unknown_integer<T>::value>::type> {
        static void put(scalar_base& s, const T& x) { s.put_(static_cast<typename known_integer<T>::type>(x)); }
    };
    template <class T, class Enable=void> struct getter {
        static T get(const scalar_base& s) { T x; s.get_(x); return x; }
    };
    template <class T> struct getter<T, typename enable_if<is_unknown_integer<T>::value>::type> {
        static T get(const scalar_base& s) { typename known_integer<T>::type x; s.get_(x); return x; }
    };

    void ok(pn_type_t) const;
    void set(const pn_atom_t&);
    void set(const binary& x, pn_type_t t);

    pn_atom_t atom_;
    binary bytes_; // Hold binary data.

    /// @cond INTERNAL
  friend class proton::message;
  friend class codec::encoder;
  friend class codec::decoder;
  template<class T> friend T get(const scalar_base& s) { return scalar_base::getter<T>::get(s); }
    /// @endcond
};

template <class R, class F> R visit(const scalar_base& s, F f) {
    switch(s.type()) {
      case BOOLEAN: return f(s.get<bool>());
      case UBYTE: return f(s.get<uint8_t>());
      case BYTE: return f(s.get<int8_t>());
      case USHORT: return f(s.get<uint16_t>());
      case SHORT: return f(s.get<int16_t>());
      case UINT: return f(s.get<uint32_t>());
      case INT: return f(s.get<int32_t>());
      case CHAR: return f(s.get<wchar_t>());
      case ULONG: return f(s.get<uint64_t>());
      case LONG: return f(s.get<int64_t>());
      case TIMESTAMP: return f(s.get<timestamp>());
      case FLOAT: return f(s.get<float>());
      case DOUBLE: return f(s.get<double>());
      case DECIMAL32: return f(s.get<decimal32>());
      case DECIMAL64: return f(s.get<decimal64>());
      case DECIMAL128: return f(s.get<decimal128>());
      case UUID: return f(s.get<uuid>());
      case BINARY: return f(s.get<binary>());
      case STRING: return f(s.get<std::string>());
      case SYMBOL: return f(s.get<symbol>());
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

} // internal
} // proton

#endif // PROTON_INTERNAL_SCALAR_BASE_HPP
