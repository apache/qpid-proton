#ifndef PROTON_CPP_MESSAGING_ADAPTER_H
#define PROTON_CPP_MESSAGING_ADAPTER_H

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

///@cond INTERNAL

struct pn_event_t;

namespace proton {

class messaging_handler;

/// Convert the low level proton-c events to the higher level proton::messaging_handler calls
class messaging_adapter
{
  public:
    static void dispatch(messaging_handler& delegate, pn_event_t* e);
};

}
///@endcond INTERNAL
#endif  /*!PROTON_CPP_MESSAGING_ADAPTER_H*/
