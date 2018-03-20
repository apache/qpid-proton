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

#include "proton_bits.hpp"

#include "proton/link.hpp"
#include "proton/error.hpp"
#include "proton/connection.hpp"

#include <proton/connection.h>
#include <proton/session.h>
#include <proton/link.h>

#include "contexts.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"

namespace proton {

void link::attach() {
    pn_link_open(pn_object());
}

void link::close() {
    pn_link_close(pn_object());
}

void link::detach() {
    pn_link_detach(pn_object());
}

int link::credit() const {
    pn_link_t *lnk = pn_object();
    if (pn_link_is_sender(lnk))
        return pn_link_credit(lnk);
    link_context& lctx = link_context::get(lnk);
    return pn_link_credit(lnk) + lctx.pending_credit;
}

bool link::draining() {
    pn_link_t *lnk = pn_object();
    link_context& lctx = link_context::get(lnk);
    if (pn_link_is_sender(lnk))
        return pn_link_credit(lnk) > 0 && lctx.draining;
    else
        return lctx.draining;
}

std::string link::name() const { return str(pn_link_name(pn_object()));}

container& link::container() const {
    return connection().container();
}

work_queue& link::work_queue() const {
    return connection().work_queue();
}

class connection link::connection() const {
    return make_wrapper(pn_session_connection(pn_link_session(pn_object())));
}

class session link::session() const {
    return make_wrapper(pn_link_session(pn_object()));
}

error_condition link::error() const {
    return make_wrapper(pn_link_remote_condition(pn_object()));
}
}
