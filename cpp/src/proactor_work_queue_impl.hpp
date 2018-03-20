#ifndef PROTON_CPP_EVENT_LOOP_IMPL_HPP
#define PROTON_CPP_EVENT_LOOP_IMPL_HPP

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

#include "proton/fwd.hpp"

namespace proton {

class work_queue::impl {
  public:
    virtual ~impl() {};
    virtual bool add(work f) = 0;
    void add_void(work f) { add(f); }
    virtual void schedule(duration, work) = 0;
    virtual void run_all_jobs() = 0;
    virtual void finished() = 0;
};

}

#endif // PROTON_CPP_EVENT_LOOP_IMPL_HPP
