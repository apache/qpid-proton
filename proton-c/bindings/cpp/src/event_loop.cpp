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

#include "proton/event_loop.hpp"

#include "contexts.hpp"
#include "proactor_container_impl.hpp"
#include "proactor_event_loop_impl.hpp"

#include <proton/session.h>
#include <proton/link.h>

namespace proton {

event_loop::event_loop() {}
event_loop::event_loop(container& c) { *this = container::impl::make_event_loop(c); }

event_loop::~event_loop() {}

event_loop& event_loop::operator=(impl* i) { impl_.reset(i); return *this; }

bool event_loop::inject(void_function0& f) {
    return impl_->inject(f);
}

#if PN_CPP_HAS_STD_FUNCTION
bool event_loop::inject(std::function<void()> f) {
    return impl_->inject(f);
}
#endif

event_loop& event_loop::get(pn_connection_t* c) {
    return connection_context::get(c).event_loop_;
}

event_loop& event_loop::get(pn_session_t* s) {
    return get(pn_session_connection(s));
}

event_loop& event_loop::get(pn_link_t* l) {
    return get(pn_link_session(l));
}

}
