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

#include "proton/work_queue.hpp"

#include "contexts.hpp"
#include "proactor_container_impl.hpp"
#include "proactor_work_queue_impl.hpp"

#include <proton/session.h>
#include <proton/link.h>

namespace proton {

work_queue::work_queue() {}
work_queue::work_queue(container& c) { *this = container::impl::make_work_queue(c); }

work_queue::~work_queue() {}

work_queue& work_queue::operator=(impl* i) { impl_.reset(i); return *this; }

bool work_queue::add(work f) {
    return impl_->add(f);
}

work_queue& work_queue::get(pn_connection_t* c) {
    return connection_context::get(c).work_queue_;
}

work_queue& work_queue::get(pn_session_t* s) {
    return get(pn_session_connection(s));
}

work_queue& work_queue::get(pn_link_t* l) {
    return get(pn_link_session(l));
}

}
