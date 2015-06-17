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

#include "proton/ImportExport.hpp"
#include <iosfwd>

/**@file
 * Base for classes that hold AMQP data.
 * @internal
 */
struct pn_data_t;

namespace proton {

/** Base for classes that hold AMQP data. */
PN_CPP_EXTERN class Data {
  public:
    explicit Data();
    virtual ~Data();
    Data(const Data&);

    Data& operator=(const Data&);

    /** Clear the data. */
    void clear();

    /** True if there are no values. */
    bool empty() const;

    /** Human readable representation of data. */
    friend std::ostream& operator<<(std::ostream&, const Data&);

    /** The underlying pn_data_t */
    pn_data_t* pnData() { return data; }

    /** True if this Data object owns it's own pn_data_t, false if it is acting as a "view" */
    bool own() const { return own_; }

    void swap(Data&);

  protected:
    /** Does not take ownership, just a view on the data */
    explicit Data(pn_data_t*);

    /** Does not take ownership, just a view on the data */
    void view(pn_data_t*);

    mutable pn_data_t* data;
    bool own_;
};


}
#endif // DATA_H
