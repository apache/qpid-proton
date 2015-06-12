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

#include "proton/cpp/ImportExport.h"
#include <iosfwd>

/**@file
 * Base for classes that hold AMQP data.
 * @internal
 */
struct pn_data_t;

namespace proton {
namespace reactor {

/** Base for classes that hold AMQP data. */
class Data {
  public:
    virtual ~Data();

    /** Copies the data */
    Data& operator=(const Data&);

    /** Clear the data. */
    PN_CPP_EXTERN void clear();

    /** True if there are no values. */
    PN_CPP_EXTERN bool empty() const;

    /** Human readable representation of data. */
    friend std::ostream& operator<<(std::ostream&, const Data&);

  protected:
    /** Takes ownership of pd */
    explicit Data(pn_data_t* pd=0);
    mutable pn_data_t* data;
};


}}
#endif // DATA_H
