#ifndef VALUES_H
#define VALUES_H
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

#include <proton/encoder.hpp>
#include <proton/decoder.hpp>

namespace proton {


/** Holds a sequence of AMQP values, allows inserting and extracting.
 *
 * After inserting values, call rewind() to extract them.
 */
class values : public encoder, public decoder {
  public:
    PN_CPP_EXTERN values();
    PN_CPP_EXTERN values(const values&);

    /** Does not take ownership, just operates on the pn_data_t object. */
    PN_CPP_EXTERN values(pn_data_t*);

    PN_CPP_EXTERN ~values();

    /** Copy data from another values */
    PN_CPP_EXTERN values& operator=(const values&);

  friend class value;
  friend class message;
};

PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const values&);

}

#endif // VALUES_H
