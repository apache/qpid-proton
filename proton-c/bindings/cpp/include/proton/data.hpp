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

#include "proton/decoder.hpp"
#include "proton/encoder.hpp"
#include "proton/export.hpp"
#include "proton/object.hpp"
#include "proton/pn_unique_ptr.hpp"

#include <iosfwd>

struct pn_data_t;

namespace proton {

class data;

/**
 * Holds a sequence of AMQP values, allows inserting and extracting via encoder() and decoder().
 * Cannot be directly instantiated, use `value`
 */
class data : public object<pn_data_t> {
  public:
    data(pn_data_t* d) : object(d) {}
    data(owned_object<pn_data_t> d) : object(d) {}

    PN_CPP_EXTERN static owned_object<pn_data_t> create();

    PN_CPP_EXTERN data& operator=(const data&);

    template<class T> data& operator=(const T &t) {
        clear(); encoder() << t; return *this;
    }

    /** Clear the data. */
    PN_CPP_EXTERN void clear();

    /** True if there are no values. */
    PN_CPP_EXTERN bool empty() const;

    /** Encoder to encode into this value */
    PN_CPP_EXTERN class encoder encoder();

    /** Decoder to decode from this value */
    PN_CPP_EXTERN class decoder decoder();

    /** Return the data cursor position */
    PN_CPP_EXTERN uintptr_t point() const;

    /** Restore the cursor position to a previously saved position */
    PN_CPP_EXTERN void restore(uintptr_t h);

    PN_CPP_EXTERN void narrow();

    PN_CPP_EXTERN void widen();

    PN_CPP_EXTERN int append(data src);

    PN_CPP_EXTERN int appendn(data src, int limit);

    PN_CPP_EXTERN bool next() const;

    /** Rewind and return the type of the first value*/
    PN_CPP_EXTERN type_id type() const;

    /** Rewind and decode the first value */
    template<class T> void get(T &t) const { decoder().rewind(); decoder() >> t; }

    template<class T> T get() const { T t; get(t); return t; }

    PN_CPP_EXTERN bool operator==(const data& x) const;
    PN_CPP_EXTERN bool operator<(const data& x) const;

    /** Human readable representation of data. */
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const data&);
  friend class value;
  private:
    class decoder decoder() const { return const_cast<data*>(this)->decoder(); }
};

}
#endif // DATA_H

