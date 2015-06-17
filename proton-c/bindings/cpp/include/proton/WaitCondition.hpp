#ifndef PROTON_CPP_WAITCONDITION_H
#define PROTON_CPP_WAITCONDITION_H

/*
 *
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
 *
 */
#include "proton/ImportExport.hpp"

namespace proton {
namespace reactor {

// Interface class to indicates that an expected contion has been
// achieved, i.e. for BlockingConnection.wait()

class WaitCondition
{
  public:
    PN_CPP_EXTERN virtual ~WaitCondition();

    // Overide this member function to indicate whether an expected
    // condition is achieved and requires no further waiting.
    virtual bool achieved() = 0;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_WAITCONDITION_H*/
