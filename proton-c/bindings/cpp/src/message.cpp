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

#include "proton/message.hpp"
#include "proton/error.hpp"
#include "proton/message.h"
#include "msg.hpp"
#include "proton_bits.hpp"
#include "proton_impl_ref.hpp"

#include <cstring>

namespace proton {

template class proton_handle<pn_message_t>;

typedef proton_impl_ref<message> PI;

message::message() : body_(0) {
    PI::ctor(*this, 0);
}
message::message(pn_message_t *p) : body_(0) {
    PI::ctor(*this, p);
}
message::message(const message& m) : proton_handle<pn_message_t>(), body_(0) {
    PI::copy(*this, m);
}

// TODO aconway 2015-06-17: message should be a value not a handle
// Needs to own pn_message_t and do appropriate _copy and _free operations.

message& message::operator=(const message& m) {
    return PI::assign(*this, m);
}
message::~message() { PI::dtor(*this); }

void message::clear() { pn_message_clear(impl_); }

namespace {
void confirm(pn_message_t * const&  p) {
    if (p) return;
    const_cast<pn_message_t*&>(p) = pn_message(); // Correct refcount of 1
    if (!p)
        throw error(MSG("No memory"));
}

void check(int err) {
    if (err) throw error(error_str(err));
}

void set_value(pn_data_t* d, const value& v) {
    values values(d);
    values.clear();
    values << v;
}

value get_value(pn_data_t* d) {
    values values(d);
    values.rewind();
    return values.get<value>();
}
} // namespace

void message::id(const value& id) {
    confirm(impl_);
    set_value(pn_message_id(impl_), id);
}

value message::id() const {
    confirm(impl_);
    return get_value(pn_message_id(impl_));
}
void message::user(const std::string &id) {
    confirm(impl_);
    check(pn_message_set_user_id(impl_, pn_bytes(id)));
}

std::string message::user() const {
    confirm(impl_);
    return str(pn_message_get_user_id(impl_));
}

void message::address(const std::string &addr) {
    confirm(impl_);
    check(pn_message_set_address(impl_, addr.c_str()));
}

std::string message::address() const {
    confirm(impl_);
    const char* addr = pn_message_get_address(impl_);
    return addr ? std::string(addr) : std::string();
}

void message::subject(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_subject(impl_, s.c_str()));
}

std::string message::subject() const {
    confirm(impl_);
    const char* s = pn_message_get_subject(impl_);
    return s ? std::string(s) : std::string();
}

void message::reply_to(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_reply_to(impl_, s.c_str()));
}

std::string message::reply_to() const {
    confirm(impl_);
    const char* s = pn_message_get_reply_to(impl_);
    return s ? std::string(s) : std::string();
}

void message::correlation_id(const value& id) {
    confirm(impl_);
    set_value(pn_message_correlation_id(impl_), id);
}

value message::correlation_id() const {
    confirm(impl_);
    return get_value(pn_message_correlation_id(impl_));
}

void message::content_type(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_content_type(impl_, s.c_str()));
}

std::string message::content_type() const {
    confirm(impl_);
    const char* s = pn_message_get_content_type(impl_);
    return s ? std::string(s) : std::string();
}

void message::content_encoding(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_content_encoding(impl_, s.c_str()));
}

std::string message::content_encoding() const {
    confirm(impl_);
    const char* s = pn_message_get_content_encoding(impl_);
    return s ? std::string(s) : std::string();
}

void message::expiry(amqp_timestamp t) {
    confirm(impl_);
    pn_message_set_expiry_time(impl_, t.milliseconds);
}
amqp_timestamp message::expiry() const {
    confirm(impl_);
    return amqp_timestamp(pn_message_get_expiry_time(impl_));
}

void message::creation_time(amqp_timestamp t) {
    confirm(impl_);
    pn_message_set_creation_time(impl_, t);
}
amqp_timestamp message::creation_time() const {
    confirm(impl_);
    return pn_message_get_creation_time(impl_);
}

void message::group_id(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_group_id(impl_, s.c_str()));
}

std::string message::group_id() const {
    confirm(impl_);
    const char* s = pn_message_get_group_id(impl_);
    return s ? std::string(s) : std::string();
}

void message::reply_to_group_id(const std::string &s) {
    confirm(impl_);
    check(pn_message_set_reply_to_group_id(impl_, s.c_str()));
}

std::string message::reply_to_group_id() const {
    confirm(impl_);
    const char* s = pn_message_get_reply_to_group_id(impl_);
    return s ? std::string(s) : std::string();
}

void message::body(const value& v) {
    confirm(impl_);
    set_value(pn_message_body(impl_), v);
}

void message::body(const values& v) {
    confirm(impl_);
    pn_data_copy(pn_message_body(impl_), v.data_);
}

const values& message::body() const {
    confirm(impl_);
    body_.view(pn_message_body(impl_));
    return body_;
}

values& message::body() {
    confirm(impl_);
    body_.view(pn_message_body(impl_));
    return body_;
}

void message::encode(std::string &s) {
    confirm(impl_);
    size_t sz = s.capacity();
    if (sz < 512) sz = 512;
    while (true) {
        s.resize(sz);
        int err = pn_message_encode(impl_, (char *) s.data(), &sz);
        if (err) {
            if (err != PN_OVERFLOW)
                check(err);
        } else {
            s.resize(sz);
            return;
        }
        sz *= 2;
    }
}

void message::decode(const std::string &s) {
    confirm(impl_);
    check(pn_message_decode(impl_, s.data(), s.size()));
}

pn_message_t *message::pn_message() const {
    return impl_;
}

}
