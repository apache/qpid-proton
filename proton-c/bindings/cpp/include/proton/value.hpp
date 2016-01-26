#ifndef VALUE_H
#define VALUE_H

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

#include "proton/data.hpp"
#include "proton/types.hpp"

namespace proton {

/// A holder for an AMQP value.
///
/// A proton::value can hold any AMQP data value, simple or compound.
/// It has assignment and conversion operators to convert its contents
/// easily to and from native C++ types.
class value : public comparable<value> {
  public:
    /// Create an empty value.
    PN_CPP_EXTERN value();

    /// Copy a value.
    PN_CPP_EXTERN value(const value&);

#if PN_HAS_CPP11
    PN_CPP_EXTERN value(value&&);
#endif

    /// Copy a value.
    PN_CPP_EXTERN value& operator=(const value&);

    /// Create a value from C++ type T.
    template <class T> value(const T& x) : data_(proton::data::create()) { encode() << x; }

    /// Create a value from C++ type T.
    template <class T> value& operator=(const T& x) { encode() << x; return *this; }

    /// Remove any contained data.
    PN_CPP_EXTERN void clear();

    /// True if the value contains no data.
    PN_CPP_EXTERN bool empty() const;

    /// Get the type of the current value.
    PN_CPP_EXTERN type_id type() const;

    /// @name Get methods
    ///
    /// Extract the value to type T.
    ///
    /// @{

    /// Get the value.
    template<class T> void get(T &t) const { decode() >> t; }

    /// Get an AMQP map as any type T that satisfies the map concept.
    template<class T> void get_map(T& t) const { decode() >> to_map(t); }

    /// Get a map as a as any type T that is a sequence pair-like types with first and second.
    template<class T> void get_pairs(T& t) const { decode() >> to_pairs(t); }

    /// Get an AMQP array or list as type T that satisfies the sequence concept. */
    template<class T> void get_sequence(T& t) const { decode() >> to_sequence(t); }

    /// @}

    /// Get the value as C++ type T.
    template<class T> T get() const { T t; get(t); return t; }

    /// @name As methods
    ///
    /// As methods do "loose" conversion, they will convert the scalar
    /// value to the requested type if possible, else throw type_error.
    ///
    /// @{
    PN_CPP_EXTERN int64_t as_int() const;        ///< Allowed if `type_id_is_integral(type())`
    PN_CPP_EXTERN uint64_t as_uint() const;      ///< Allowed if `type_id_is_integral(type())`
    PN_CPP_EXTERN double as_double() const;      ///< Allowed if `type_id_is_floating_point(type())`
    PN_CPP_EXTERN std::string as_string() const; ///< Allowed if `type_id_is_string_like(type())`
    /// @}

    /// @cond INTERNAL
    /// XXX undiscussed
    PN_CPP_EXTERN encoder encode();              ///< Clear and return an encoder for this value.
    PN_CPP_EXTERN decoder decode() const;        ///< Rewind and return an encoder for this value.
    PN_CPP_EXTERN class data& data() const;      ///< Return a data reference, no clear or rewind.
    /// @endcond

  private:
    mutable class data data_;

    /// @cond INTERNAL
    friend PN_CPP_EXTERN void swap(value&, value&);
    friend PN_CPP_EXTERN bool operator==(const value& x, const value& y);
    friend PN_CPP_EXTERN bool operator<(const value& x, const value& y);
    friend PN_CPP_EXTERN class encoder operator<<(class encoder e, const value& dv);
    friend PN_CPP_EXTERN class decoder operator>>(class decoder d, value& dv);
    friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, const value& dv);
    friend class message;
    /// @endcond
};

}

#endif // VALUE_H
