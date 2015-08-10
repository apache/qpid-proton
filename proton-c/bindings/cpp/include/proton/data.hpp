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
#include <iosfwd>

struct pn_data_t;

namespace proton {

/** Base for classes that hold AMQP data, not for direct use. @see value, values, encoder, decoder. */
class data {
  public:
    PN_CPP_EXTERN explicit data();
    PN_CPP_EXTERN data(const data&);
    PN_CPP_EXTERN virtual ~data();
    PN_CPP_EXTERN data& operator=(const data&);

    /** Clear the data. */
    PN_CPP_EXTERN void clear();

    /** Rewind to the start of the data. */
    PN_CPP_EXTERN void rewind();

    /** True if there are no values. */
    PN_CPP_EXTERN bool empty() const;

    /** The underlying pn_data_t */
    PN_CPP_EXTERN pn_data_t* pn_data() { return data_; }

    /** True if this data object owns it's own pn_data_t, false if it is acting as a "view" */
    PN_CPP_EXTERN bool own() const { return own_; }

    PN_CPP_EXTERN void swap(data&);

    /** Human readable representation of data. */
    friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const data&);

  protected:
    /** Does not take ownership, just a view on the data */
    PN_CPP_EXTERN explicit data(pn_data_t*);

    /** Does not take ownership, just a view on the data */
    PN_CPP_EXTERN  void view(pn_data_t*);

    mutable pn_data_t* data_;
    bool own_;
};


}
#endif // DATA_H
