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

#include "proton/Values.hpp"

struct pn_data_t;

/**@file
 * Holder for an AMQP value.
 * @ingroup cpp
 */
namespace proton {

/** Holds a single AMQP value. */
class Value {
  public:
    PN_CPP_EXTERN Value();
    PN_CPP_EXTERN Value(const Value&);

    /** Converting constructor from any settable value */
    template <class T> explicit Value(const T& v);

    PN_CPP_EXTERN ~Value();

    PN_CPP_EXTERN Value& operator=(const Value&);

    /** Copy the first value from a raw pn_data_t. */
    PN_CPP_EXTERN Value& operator=(pn_data_t*);

    PN_CPP_EXTERN TypeId type() const;

    /** Set the value. */
    template<class T> void set(const T& value);
    /** Get the value. */
    template<class T> void get(T& value) const;
    /** Get the value */
    template<class T> T get() const;

    /** Assignment sets the value */
    template<class T> Value& operator=(const T& value);

    /** Conversion operator gets  the value */
    template<class T> operator T() const;

    /** insert a value into an Encoder. */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, const Value&);

    /** Extract a value from a decoder. */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, Value&);

    /** Human readable format */
    PN_CPP_EXTERN friend std::ostream& operator<<(std::ostream&, const Value&);

    PN_CPP_EXTERN bool operator==(const Value&) const;
    PN_CPP_EXTERN bool operator !=(const Value& v) const{ return !(*this == v); }

    /** operator < makes Value valid for use as a std::map key. */
    PN_CPP_EXTERN bool operator<(const Value&) const;
    bool operator>(const Value& v) const { return v < *this; }
    bool operator<=(const Value& v) const { return !(*this > v); }
    bool operator>=(const Value& v) const { return !(*this < v); }

  private:
    mutable Values values;
};

template<class T> void Value::set(const T& value) {
    values.clear();
    values << value;
}

template<class T> void Value::get(T& value) const {
    Values& v = const_cast<Values&>(values);
    v.rewind() >> value;
}

template<class T> T Value::get() const { T value; get(value); return value; }

template<class T> Value& Value::operator=(const T& value) { set(value); return *this; }

template<class T> Value::operator T() const { return get<T>(); }

template<class T> Value::Value(const T& value) { set(value); }
}

#endif // VALUE_H
