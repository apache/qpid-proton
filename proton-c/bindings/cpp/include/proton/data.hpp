#ifndef DATA_H
#define DATA_H
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

#include "proton/export.hpp"
#include "proton/facade.hpp"
#include "proton/decoder.hpp"
#include "proton/encoder.hpp"
#include "proton/memory.hpp"
#include <iosfwd>

struct pn_data_t;

namespace proton {

class data;

/**
 * Holds a sequence of AMQP values, allows inserting and extracting via encoder() and decoder().
 * Cannot be directly instantiated, use `data_value`
 */
class data : public facade<pn_data_t, data, comparable<data> > {
  public:
    PN_CPP_EXTERN static PN_UNIQUE_PTR<data> create();

    PN_CPP_EXTERN data& operator=(const data&);
    template<class T> data& operator=(const T &t) {
        clear(); encoder() << t; decoder().rewind(); return *this;
    }

    /** Clear the data. */
    PN_CPP_EXTERN void clear();

    /** True if there are no values. */
    PN_CPP_EXTERN bool empty() const;

    /** Encoder to encode into this value */
    PN_CPP_EXTERN class encoder& encoder();

    /** Decoder to decode from this value */
    PN_CPP_EXTERN class decoder& decoder();

    /** Type of the current value*/
    PN_CPP_EXTERN type_id type() { return decoder().type(); }

    /** Get the current value, don't move the decoder pointer. */
    template<class T> void get(T &t) { decoder() >> t; decoder().backup(); }

    /** Get the current value */
    template<class T> T get() { T t; get(t); return t; }

    PN_CPP_EXTERN bool operator==(const data& x) const;
    PN_CPP_EXTERN bool operator<(const data& x) const;

    PN_CPP_EXTERN void operator delete(void *);

    /** Human readable representation of data. */
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const data&);
};

/** data with normal value semantics: copy, assign etc. */
class data_value {
  public:
    data_value() : data_(data::create()) {}
    data_value(const data_value& x) : data_(data::create()) { *data_ = *x.data_; }
    data_value(const data& x) : data_(data::create()) { *data_ = x; }
    template <class T> data_value(const T& x) : data_(data::create()) { *data_ = x; }

    operator data&() { return *data_; }
    operator const data&() const { return *data_; }

    data_value& operator=(const data_value& x) { *data_ = *x.data_; return *this; }
    data_value& operator=(const data& x) { *data_ = x; return *this; }
    template <class T> data_value& operator=(const T& x) { *data_ = x; return *this; }

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

    bool operator==(const data_value& x) const { return *data_ == *x.data_; }
    bool operator<(const data_value& x) const { return *data_ < *x.data_; }

  friend inline class encoder& operator<<(class encoder& e, const data_value& dv) {
      return e << *dv.data_;
  }
  friend inline class decoder& operator>>(class decoder& d, data_value& dv) {
      return d >> *dv.data_;
  }
  friend inline std::ostream& operator<<(std::ostream& o, const data_value& dv) {
      return o << *dv.data_;
  }
  private:
    PN_UNIQUE_PTR<data> data_;
};


}
#endif // DATA_H
