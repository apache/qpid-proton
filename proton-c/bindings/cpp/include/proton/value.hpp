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

namespace proton {

/** AMQP data  with normal value semantics: copy, assign etc. */
class value {
  public:
    value() : data_(data::create()) {}
    value(const value& x) : data_(data::create()) { *data_ = *x.data_; }
    value(const data& x) : data_(data::create()) { *data_ = x; }
    template <class T> value(const T& x) : data_(data::create()) { *data_ = x; }

    operator data&() { return *data_; }
    operator const data&() const { return *data_; }

    value& operator=(const value& x) { *data_ = *x.data_; return *this; }
    value& operator=(const data& x) { *data_ = x; return *this; }
    template <class T> value& operator=(const T& x) { *data_ = x; return *this; }

    void clear() { data_->clear(); }
    bool empty() const { return data_->empty(); }

    /** Encoder to encode into this value */
    class encoder& encoder() { return data_->encoder(); }

    /** Decoder to decode from this value */
    class decoder& decoder() { return data_->decoder(); }

    /** Type of the current value*/
    type_id type() { return decoder().type(); }

    /** Get the current value, don't move the decoder pointer. */
    template<class T> void get(T &t) { decoder() >> t; decoder().backup(); }

    /** Get the current value */
    template<class T> T get() { T t; get(t); return t; }
    template<class T> operator T() { return get<T>(); }

    bool operator==(const value& x) const { return *data_ == *x.data_; }
    bool operator<(const value& x) const { return *data_ < *x.data_; }

  friend inline class encoder& operator<<(class encoder& e, const value& dv) {
      return e << *dv.data_;
  }
  friend inline class decoder& operator>>(class decoder& d, value& dv) {
      return d >> *dv.data_;
  }
  friend inline std::ostream& operator<<(std::ostream& o, const value& dv) {
      return o << *dv.data_;
  }
  private:
    pn_unique_ptr<data> data_;
};


}
#endif // VALUE_H

