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

namespace proton {

void session::open() {
    pn_session_open(pn_object());
}

connection session::connection() const {
    return pn_session_connection(pn_object());
}

namespace {
std::string set_name(const std::string& name, session* s) {
    if (name.empty())
        return s->connection().context().container_impl->next_link_name();
    return name;
}
}

receiver session::create_receiver(const std::string& name) {
    return pn_receiver(pn_object(), set_name(name, this).c_str());
}

sender session::create_sender(const std::string& name) {
    return pn_sender(pn_object(), set_name(name, this).c_str());
}

sender session::open_sender(const std::string &addr, handler *h) {
    sender snd = create_sender();
    snd.target().address(addr);
    if (h) snd.handler(*h);
    snd.open();
    return snd;
}

receiver session::open_receiver(const std::string &addr, bool dynamic, handler *h)
{
    receiver rcv = create_receiver();
    rcv.source().address(addr);
    if (dynamic) rcv.source().dynamic(true);
    if (h) rcv.handler(*h);
    rcv.open();
    return rcv;
}

session session::next(endpoint::state s) const
{
    return pn_session_next(pn_object(), s);
}

endpoint::state session::state() const { return pn_session_state(pn_object()); }

link_range session::find_links(endpoint::state mask)  const {
    link_range r(connection().find_links(mask));
    link_iterator i(r.begin(), *this);
    if (i && *this != i->session())
        ++i;
    return link_range(i);
}

} // namespace proton

