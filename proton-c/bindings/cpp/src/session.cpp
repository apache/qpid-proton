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
#include "proton/session.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/session.hpp"
#include "proton/connection.hpp"

#include "contexts.hpp"
#include "container_impl.hpp"

#include <string>

namespace proton {

void session::open() {
    pn_session_open(pn_object());
}

connection session::connection() const {
    return pn_session_connection(pn_object());
}

namespace {
std::string link_name(const std::string& name, session* s) {
    if (!name.empty()) return name;
    std::string gen(connection_context::get(s->connection()).link_gen.next());
    return gen;
}
}

receiver session::create_receiver(const std::string& name) {
    return pn_receiver(pn_object(), link_name(name, this).c_str());
}

sender session::create_sender(const std::string& name) {
    return pn_sender(pn_object(), link_name(name, this).c_str());
}

sender session::open_sender(const std::string &addr, const link_options &lo) {
    sender snd = create_sender();
    snd.local_target().address(addr);
    snd.open(lo);
    return snd;
}

receiver session::open_receiver(const std::string &addr, const link_options &lo)
{
    receiver rcv = create_receiver();
    rcv.local_source().address(addr);
    rcv.open(lo);
    return rcv;
}

endpoint::state session::state() const { return pn_session_state(pn_object()); }

condition session::local_condition() const {
    return condition(pn_session_condition(pn_object()));
}

condition session::remote_condition() const {
    return condition(pn_session_remote_condition(pn_object()));
}

link_range session::links()  const {
    link_range r(connection().links());
    if (r.empty()) return r;
    link_iterator i(*r.begin(), pn_object());
    if (*this != (*i).session()) ++i;
    return link_range(i);
}

session_iterator session_iterator::operator++() {
    obj_ = pn_session_next(obj_.pn_object(), 0);
    return *this;
}

} // namespace proton
