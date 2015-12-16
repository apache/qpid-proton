#ifndef ATOM_HPP
#define ATOM_HPP
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

#include "proton/types.hpp"
#include <iosfwd>

namespace proton {

class atom;

/** atom holds an instance of an atomic proton type. */
class atom : public comparable<atom> {
  public:
    PN_CPP_EXTERN atom();
    // Use default assign and copy.

    /// Type for the value in the atom, NULL_TYPE if empty()
    PN_CPP_EXTERN type_id type() const;
    /// True if the atom is empty.
    PN_CPP_EXTERN bool empty() const;

    ///@name Create an atom, type() is deduced from the C++ type of the value.
    ///@{
    PN_CPP_EXTERN explicit atom(bool);
    PN_CPP_EXTERN explicit atom(uint8_t);
    PN_CPP_EXTERN explicit atom(int8_t);
    PN_CPP_EXTERN explicit atom(uint16_t);
    PN_CPP_EXTERN explicit atom(int16_t);
    PN_CPP_EXTERN explicit atom(uint32_t);
    PN_CPP_EXTERN explicit atom(int32_t);
    PN_CPP_EXTERN explicit atom(uint64_t);
    PN_CPP_EXTERN explicit atom(int64_t);
    PN_CPP_EXTERN explicit atom(wchar_t);
    PN_CPP_EXTERN explicit atom(float);
    PN_CPP_EXTERN explicit atom(double);
    PN_CPP_EXTERN explicit atom(amqp_timestamp);
    PN_CPP_EXTERN explicit atom(const amqp_decimal32&);
    PN_CPP_EXTERN explicit atom(const amqp_decimal64&);
    PN_CPP_EXTERN explicit atom(const amqp_decimal128&);
    PN_CPP_EXTERN explicit atom(const amqp_uuid&);
    PN_CPP_EXTERN explicit atom(const amqp_string&);
    PN_CPP_EXTERN explicit atom(const amqp_symbol&);
    PN_CPP_EXTERN explicit atom(const amqp_binary&);
    PN_CPP_EXTERN explicit atom(const std::string& s); ///< Treated as an AMQP string
    PN_CPP_EXTERN explicit atom(const char* s);        ///< Treated as an AMQP string
    ///@}

    /// Assign to an atom using the same rules as construction.
    template <class T> atom& operator=(T x) { return *this = atom(x); }

    ///@name get(T&) extracts the value if the types match exactly,
    ///i.e. if `type() == type_id_of<T>::value`
    /// throws type_mismatch otherwise.
    ///@{
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
    ///@}

    ///@ get<T>() is like get(T&) but returns the value..
    template<class T> T get() const { T x; get(x); return x; }

    ///@name as_ methods do "loose" conversion, they will convert the atom's
    ///value to the requested type if possible, else throw type_mismatch
    ///@{
    PN_CPP_EXTERN int64_t as_int() const;     ///< Allowed if type_id_integral(type())
    PN_CPP_EXTERN uint64_t as_uint() const;   ///< Allowed if type_id_integral(type())
    PN_CPP_EXTERN double as_double() const;    ///< Allowed if type_id_floating_point(type())
    PN_CPP_EXTERN std::string as_string() const; ///< Allowed if type_id_string_like(type())
    ///@}

    PN_CPP_EXTERN bool operator==(const atom& x) const;
    /// Note if the values are of different type(), operator< will compare the type()
    PN_CPP_EXTERN bool operator<(const atom& x) const;

  PN_CPP_EXTERN friend std::ostream& operator<<(std::ostream&, const atom&);

  private:
    void ok(pn_type_t) const;
    void set(const std::string&);
    pn_atom_t atom_;
    std::string str_;           // Owner of string-like data.
};

}
#endif // ATOM_HPP
