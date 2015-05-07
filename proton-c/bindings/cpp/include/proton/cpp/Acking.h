#ifndef PROTON_CPP_ACKING_H
#define PROTON_CPP_ACKING_H

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

#include "proton/cpp/ImportExport.h"
#include "proton/cpp/Delivery.h"

namespace proton {
namespace reactor {


class Acking
{
  public:
    PROTON_CPP_EXTERN virtual void accept(Delivery &d);
    PROTON_CPP_EXTERN virtual void reject(Delivery &d);
    PROTON_CPP_EXTERN virtual void release(Delivery &d, bool delivered=true);
    PROTON_CPP_EXTERN virtual void settle(Delivery &d, Delivery::state s = Delivery::REJECTED);
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_ACKING_H*/
