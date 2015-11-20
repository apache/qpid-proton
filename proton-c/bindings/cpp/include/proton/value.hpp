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
#include "proton/decoder.hpp"
#include "proton/types.hpp"

namespace proton {

class data;
class encoder;
class decoder;

/**
 * Holder for an AMQP value.
 *
 * proton::value can hold any AMQP data value, simple or compound.  It has
 * assignment and conversion operators to convert its contents easily to and
 * from native C++ types.
 *
 * See proton::encoder and proton::decoder for details of the conversion rules.
 * Assigning to a proton::value follows the encoder rules, converting from a
 * proton::value (or calling proton::value::get) follows the decoder rules.
 */
class value : public comparable<value> {
  public:
    PN_CPP_EXTERN value();
    PN_CPP_EXTERN value(const value& x);
    // TODO: Should enumerate specifically all the pointer types that can convert to value
    // to avoid accidental conversions to bool this will require enable_if<> or the like
    template <class T> value(const T& x) : data_(data::create()) { data_ = x; }

    PN_CPP_EXTERN value& operator=(const value& x);
    template <class T> value& operator=(const T& x) { data_ = x; return *this; }

    PN_CPP_EXTERN void clear();
    PN_CPP_EXTERN bool empty() const;

    /** Encoder to encode complex data into this value. Note this clears the value. */
    PN_CPP_EXTERN class encoder encoder();

    /** Decoder to decode complex data from this value. Note this rewinds the decoder. */
    PN_CPP_EXTERN class decoder decoder() const;

    /** Type of the current value*/
    PN_CPP_EXTERN type_id type() const;

    /** Get the value. */
    template<class T> void get(T &t) const { decoder() >> t; }

    /** Get the value. */
    template<class T> T get() const { T t; get(t); return t; }

    PN_CPP_EXTERN bool operator==(const value& x) const;
    PN_CPP_EXTERN bool operator<(const value& x) const;

  friend PN_CPP_EXTERN class encoder operator<<(class encoder e, const value& dv);
  friend PN_CPP_EXTERN class decoder operator>>(class decoder d, value& dv);
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream& o, const value& dv);

  private:
    data data_;
  friend class message;
};


}
#endif // VALUE_H

