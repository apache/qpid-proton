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

#include <proton/connection.h>
#include <proton/session.h>
#include "proton/session.hpp"
#include "proton/connection.hpp"
#include "proton/session_options.hpp"

#include "contexts.hpp"
#include "proton_bits.hpp"

#include <string>

namespace proton {

void session::open() {
    pn_session_open(pn_object());
}

void session::open(const session_options &opts) {
    opts.apply(*this);
    pn_session_open(pn_object());
}

void session::close()
{
    pn_session_close(pn_object());
}

container& session::container() const {
    return connection().container();
}

connection session::connection() const {
    return make_wrapper(pn_session_connection(pn_object()));
}

namespace {
std::string next_link_name(const connection& c) {
    io::link_namer* ln = connection_context::get(c).link_gen;

    return ln ? ln->link_name() : uuid::random().str();
}
}

sender session::open_sender(const std::string &addr) {
    return open_sender(addr, sender_options());
}

sender session::open_sender(const std::string &addr, const sender_options &so) {
    pn_link_t *lnk = pn_sender(pn_object(), next_link_name(connection()).c_str());
    pn_terminus_set_address(pn_link_target(lnk), addr.c_str());
    sender snd(make_wrapper<sender>(lnk));
    snd.open(so);
    return snd;
}

receiver session::open_receiver(const std::string &addr) {
    return open_receiver(addr, receiver_options());
}

receiver session::open_receiver(const std::string &addr, const receiver_options &ro)
{
    pn_link_t *lnk = pn_receiver(pn_object(), next_link_name(connection()).c_str());
    pn_terminus_set_address(pn_link_source(lnk), addr.c_str());
    receiver rcv(make_wrapper<receiver>(lnk));
    rcv.open(ro);
    return rcv;
}

error_condition session::error() const {
    return make_wrapper(pn_session_remote_condition(pn_object()));
}

size_t session::incoming_bytes() const {
    return pn_session_incoming_bytes(pn_object());
}

size_t session::outgoing_bytes() const {
    return pn_session_outgoing_bytes(pn_object());
}

sender_range session::senders() const {
    pn_link_t *lnk = pn_link_head(pn_session_connection(pn_object()), 0);
    while (lnk) {
        if (pn_link_is_sender(lnk) && pn_link_session(lnk) == pn_object())
            break;
        lnk = pn_link_next(lnk, 0);
    }
    return sender_range(sender_iterator(make_wrapper<sender>(lnk), pn_object()));
}

receiver_range session::receivers() const {
    pn_link_t *lnk = pn_link_head(pn_session_connection(pn_object()), 0);
    while (lnk) {
        if (pn_link_is_receiver(lnk) && pn_link_session(lnk) == pn_object())
            break;
        lnk = pn_link_next(lnk, 0);
    }
    return receiver_range(receiver_iterator(make_wrapper<receiver>(lnk), pn_object()));
}

session_iterator session_iterator::operator++() {
    obj_ = pn_session_next(unwrap(obj_), 0);
    return *this;
}

} // namespace proton
