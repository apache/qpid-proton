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

#include "proton/values.hpp"

namespace proton {

/** Holds a single AMQP value. */
class value {
  public:
    PN_CPP_EXTERN value();
    PN_CPP_EXTERN value(const value&);
    /** Converting constructor from any settable value */
    template <class T> explicit value(const T& v);

    PN_CPP_EXTERN ~value();

    PN_CPP_EXTERN value& operator=(const value&);

    PN_CPP_EXTERN type_id type() const;

    /** Set the value. */
    template<class T> void set(const T& value);
    /** Get the value. */
    template<class T> void get(T& value) const;
    /** Get the value */
    template<class T> T get() const;

    /** Assignment sets the value */
    template<class T> value& operator=(const T& value);

    /** Conversion operator gets  the value */
    template<class T> operator T() const;

    /** insert a value into an encoder. */
    PN_CPP_EXTERN friend encoder& operator<<(encoder&, const value&);

    /** Extract a value from a decoder. */
    PN_CPP_EXTERN friend decoder& operator>>(decoder&, value&);

    /** Human readable format */
    PN_CPP_EXTERN friend std::ostream& operator<<(std::ostream&, const value&);

    PN_CPP_EXTERN bool operator==(const value&) const;
    PN_CPP_EXTERN bool operator !=(const value& v) const{ return !(*this == v); }

    /** operator < makes value valid for use as a std::map key. */
    PN_CPP_EXTERN bool operator<(const value&) const;
    bool operator>(const value& v) const { return v < *this; }
    bool operator<=(const value& v) const { return !(*this > v); }
    bool operator>=(const value& v) const { return !(*this < v); }

  private:
    mutable values values_;
};

template<class T> void value::set(const T& value) {
    values_.clear();
    values_ << value;
}

template<class T> void value::get(T& v) const {
    values& vv = const_cast<values&>(values_);
    vv >> proton::rewind() >> v;
}

template<class T> T value::get() const { T value; get(value); return value; }

template<class T> value& value::operator=(const T& value) { set(value); return *this; }

template<class T> value::operator T() const { return get<T>(); }

template<class T> value::value(const T& value) { set(value); }
}

#endif // VALUE_H
