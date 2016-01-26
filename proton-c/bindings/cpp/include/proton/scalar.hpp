#ifndef SCALAR_HPP
#define SCALAR_HPP

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

#include "proton/comparable.hpp"
#include "proton/types.hpp"

#include <iosfwd>
#include <string>

namespace proton {

class encoder;
class decoder;

/// A holder for an instance of any scalar AMQP type.
class scalar : public comparable<scalar> {
  public:
    /// Create an empty scalar.
    PN_CPP_EXTERN scalar();

    /// Copy a scalar.
    PN_CPP_EXTERN scalar(const scalar&);

    /// Copy a scalar.
    PN_CPP_EXTERN scalar& operator=(const scalar&);

    /// Type for the value in the scalar, NULL_TYPE if empty()
    PN_CPP_EXTERN type_id type() const;

    /// True if the scalar is empty.
    PN_CPP_EXTERN bool empty() const;

    /// @name Assignment operators
    ///
    /// Assign a C++ value and deduce the AMQP type().
    ///
    /// @{
    PN_CPP_EXTERN scalar& operator=(bool);
    PN_CPP_EXTERN scalar& operator=(uint8_t);
    PN_CPP_EXTERN scalar& operator=(int8_t);
    PN_CPP_EXTERN scalar& operator=(uint16_t);
    PN_CPP_EXTERN scalar& operator=(int16_t);
    PN_CPP_EXTERN scalar& operator=(uint32_t);
    PN_CPP_EXTERN scalar& operator=(int32_t);
    PN_CPP_EXTERN scalar& operator=(uint64_t);
    PN_CPP_EXTERN scalar& operator=(int64_t);
    PN_CPP_EXTERN scalar& operator=(wchar_t);
    PN_CPP_EXTERN scalar& operator=(float);
    PN_CPP_EXTERN scalar& operator=(double);
    PN_CPP_EXTERN scalar& operator=(amqp_timestamp);
    PN_CPP_EXTERN scalar& operator=(const amqp_decimal32&);
    PN_CPP_EXTERN scalar& operator=(const amqp_decimal64&);
    PN_CPP_EXTERN scalar& operator=(const amqp_decimal128&);
    PN_CPP_EXTERN scalar& operator=(const amqp_uuid&);
    PN_CPP_EXTERN scalar& operator=(const amqp_string&);
    PN_CPP_EXTERN scalar& operator=(const amqp_symbol&);
    PN_CPP_EXTERN scalar& operator=(const amqp_binary&);
    PN_CPP_EXTERN scalar& operator=(const std::string& s); ///< Treated as an AMQP string
    PN_CPP_EXTERN scalar& operator=(const char* s);        ///< Treated as an AMQP string
    /// @}

    /// Create a scalar from any type that we can assign from.
    template <class T> explicit scalar(T x) { *this = x; }

    /// @name Get methods
    ///
    /// get(T&) extracts the value if the types match exactly and
    /// throws type_error otherwise.
    ///
    /// @{
    PN_CPP_EXTERN void get(bool&) const;
    PN_CPP_EXTERN void get(uint8_t&) const;
    PN_CPP_EXTERN void get(int8_t&) const;
    PN_CPP_EXTERN void get(uint16_t&) const;
    PN_CPP_EXTERN void get(int16_t&) const;
    PN_CPP_EXTERN void get(uint32_t&) const;
    PN_CPP_EXTERN void get(int32_t&) const;
    PN_CPP_EXTERN void get(uint64_t&) const;
    PN_CPP_EXTERN void get(int64_t&) const;
    PN_CPP_EXTERN void get(wchar_t&) const;
    PN_CPP_EXTERN void get(float&) const;
    PN_CPP_EXTERN void get(double&) const;
    PN_CPP_EXTERN void get(amqp_timestamp&) const;
    PN_CPP_EXTERN void get(amqp_decimal32&) const;
    PN_CPP_EXTERN void get(amqp_decimal64&) const;
    PN_CPP_EXTERN void get(amqp_decimal128&) const;
    PN_CPP_EXTERN void get(amqp_uuid&) const;
    PN_CPP_EXTERN void get(amqp_string&) const;
    PN_CPP_EXTERN void get(amqp_symbol&) const;
    PN_CPP_EXTERN void get(amqp_binary&) const;
    PN_CPP_EXTERN void get(std::string&) const; ///< Treated as an AMQP string
    /// @}

    /// get<T>() is like get(T&) but returns the value.
    template<class T> T get() const { T x; get(x); return x; }

    /// @name As methods
    ///
    /// As methods do "loose" conversion.  They will convert the
    /// scalar's value to the requested type if possible, else throw
    /// type_error.
    ///
    /// @{
    PN_CPP_EXTERN int64_t as_int() const;        ///< Allowed if type_id_is_integral(type())
    PN_CPP_EXTERN uint64_t as_uint() const;      ///< Allowed if type_id_is_integral(type())
    PN_CPP_EXTERN double as_double() const;      ///< Allowed if type_id_is_floating_point(type())
    PN_CPP_EXTERN std::string as_string() const; ///< Allowed if type_id_is_string_like(type())
    /// @}

    /// @cond INTERNAL
    
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const scalar&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const scalar&);
  friend PN_CPP_EXTERN decoder operator>>(decoder, scalar&);

    /// Scalars with different type() are considered unequal even if the values
    /// are equal as numbers or strings.
  friend PN_CPP_EXTERN bool operator==(const scalar& x, const scalar& y);

    /// For scalars of different type(), operator< sorts by order of type().
  friend PN_CPP_EXTERN bool operator<(const scalar& x, const scalar& y);

    /// @endcond
    
  private:
    void ok(pn_type_t) const;
    void set(const std::string&, pn_type_t);
    void set(const pn_atom_t&);
    pn_atom_t atom_;
    std::string str_;           // Owner of string-like data.

  friend class message;
};

/// @cond INTERNAL
/// XXX should it be public?
    
/// Base class for restricted scalar types.
class restricted_scalar : public comparable<restricted_scalar> {
  public:
    operator const scalar&() const { return scalar_; }
    type_id type() const { return scalar_.type(); }

    /// @name As methods
    ///
    /// As methods do "loose" conversion.  They will convert the
    /// scalar's value to the requested type if possible, else throw
    /// type_error.
    ///
    /// @{
    int64_t as_int() const { return scalar_.as_int(); }
    uint64_t as_uint() const { return scalar_.as_uint(); }
    double as_double() const { return scalar_.as_double();  }
    std::string as_string() const { return scalar_.as_string(); }
    /// @}

  protected:
    restricted_scalar() {}
    scalar scalar_;

    friend std::ostream& operator<<(std::ostream& o, const restricted_scalar& x)  { return o << x.scalar_; }
    friend bool operator<(const restricted_scalar& x, const restricted_scalar& y)  { return x.scalar_ < y.scalar_; }
    friend bool operator==(const restricted_scalar& x, const restricted_scalar& y)  { return x.scalar_ == y.scalar_; }
};

/// @endcond

}

#endif // SCALAR_HPP
