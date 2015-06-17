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

#include <proton/Encoder.hpp>
#include <proton/Decoder.hpp>

/**@file
 * Holder for a sequence of AMQP values.
 * @ingroup cpp
 */

namespace proton {


/** Holds a sequence of AMQP values, allows inserting and extracting.
 *
 * After inserting values, call rewind() to extract them.
 */
PN_CPP_EXTERN class Values : public Encoder, public Decoder {
  public:
    Values();
    Values(const Values&);

    /** Does not take ownership, just a view on the data */
    Values(pn_data_t*);

    ~Values();

    /** Copy data from another Values */
    Values& operator=(const Values&);

    Values& rewind();


  friend class Value;
  friend class Message;
};

PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const Values&);

}

#endif // VALUES_H
