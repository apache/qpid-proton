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

#include "proton/cpp/Encoder.h"
#include "proton/cpp/Decoder.h"
#include <iosfwd>

namespace proton {
namespace reactor {

/** Holds a sequence of AMQP values, allows inserting and extracting.
 *
 * After inserting values, call rewind() to extract them.
 */
class Values : public Encoder, public Decoder {
  public:
    Values();
    Values(const Values&);
    ~Values();

    /** Copy data from another Values */
    Values& operator=(const Values&);

    PN_CPP_EXTERN void rewind();

  private:
  friend class Value;
};

/** Holds a single AMQP value. */
class Value {
  public:
    PN_CPP_EXTERN Value();
    PN_CPP_EXTERN Value(const Value&);
    PN_CPP_EXTERN ~Value();

    PN_CPP_EXTERN Value& operator=(const Value&);

    TypeId type() const;

    /** Set the value */
    template<class T> void set(const T& value);
    /** Get the value */
    template<class T> void get(T& value) const;
    /** Get the value */
    template<class T> T get() const;

    /** Assignment sets the value */
    template<class T> Value& operator=(const T& value);
    /** Conversion operator gets  the value */
    template<class T> operator T() const;

    /** Insert a value into an Encoder. */
    PN_CPP_EXTERN friend Encoder& operator<<(Encoder&, const Value&);

    /** Extract a value from a decoder. */
    PN_CPP_EXTERN friend Decoder& operator>>(Decoder&, const Value&);

  friend Decoder& operator>>(Decoder&, Value&);
  friend Encoder& operator<<(Encoder&, const Value&);

    private:
    Values values;
};

template<class T> void Value::set(const T& value) {
    values.clear();
    values << value;
}

template<class T> void Value::get(T& value) const {
    Values& v = const_cast<Values&>(values);
    v.rewind();
    v >> value;
}

template<class T> T Value::get() const { T value; get(value); return value; }

template<class T> Value& Value::operator=(const T& value) { set(value); return *this; }

template<class T> Value::operator T() const { return get<T>(); }

}}

#endif // VALUE_H
