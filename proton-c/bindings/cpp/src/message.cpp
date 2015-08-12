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
#include "proton/link.hpp"
#include "proton/delivery.hpp"
#include "proton/message.h"
#include "proton/sender.hpp"
#include "proton/receiver.hpp"
#include "proton/delivery.h"
#include "msg.hpp"
#include "proton_bits.hpp"

#include <cstring>
#include <assert.h>

namespace proton {

message::message() : impl_(::pn_message()), body_(0) { assert(impl_); }

message::message(pn_message_t *p) : impl_(p), body_(0) { assert(impl_); }

message::message(const message& m) : impl_(::pn_message()), body_(0) { *this = m; }

message& message::operator=(const message& m) {
    // TODO aconway 2015-08-10: need more efficient pn_message_copy function
    std::string data;
    m.encode(data);
    decode(data);
    return *this;
}

message::~message() { if (impl_) pn_message_free(impl_); }

void message::clear() { pn_message_clear(impl_); }

namespace {
void check(int err) {
    if (err) throw error(error_str(err));
}

void set_value(pn_data_t* d, const value& v) {
    values values(d);
    values.clear();
    values << v;
}

value get_value(pn_data_t* d) {
    if (d) {
        values vals(d);
        vals.rewind();
        if (vals.more())
            return vals.get<value>();
    }
    return value();
}
} // namespace

void message::id(const value& id) {
    set_value(pn_message_id(impl_), id);
}

value message::id() const {
    return get_value(pn_message_id(impl_));
}
void message::user(const std::string &id) {
    check(pn_message_set_user_id(impl_, pn_bytes(id)));
}

std::string message::user() const {
    return str(pn_message_get_user_id(impl_));
}

void message::address(const std::string &addr) {
    check(pn_message_set_address(impl_, addr.c_str()));
}

std::string message::address() const {
    const char* addr = pn_message_get_address(impl_);
    return addr ? std::string(addr) : std::string();
}

void message::subject(const std::string &s) {
    check(pn_message_set_subject(impl_, s.c_str()));
}

std::string message::subject() const {
    const char* s = pn_message_get_subject(impl_);
    return s ? std::string(s) : std::string();
}

void message::reply_to(const std::string &s) {
    check(pn_message_set_reply_to(impl_, s.c_str()));
}

std::string message::reply_to() const {
    const char* s = pn_message_get_reply_to(impl_);
    return s ? std::string(s) : std::string();
}

void message::correlation_id(const value& id) {
    set_value(pn_message_correlation_id(impl_), id);
}

value message::correlation_id() const {
    return get_value(pn_message_correlation_id(impl_));
}

void message::content_type(const std::string &s) {
    check(pn_message_set_content_type(impl_, s.c_str()));
}

std::string message::content_type() const {
    const char* s = pn_message_get_content_type(impl_);
    return s ? std::string(s) : std::string();
}

void message::content_encoding(const std::string &s) {
    check(pn_message_set_content_encoding(impl_, s.c_str()));
}

std::string message::content_encoding() const {
    const char* s = pn_message_get_content_encoding(impl_);
    return s ? std::string(s) : std::string();
}

void message::expiry(amqp_timestamp t) {
    pn_message_set_expiry_time(impl_, t.milliseconds);
}
amqp_timestamp message::expiry() const {
    return amqp_timestamp(pn_message_get_expiry_time(impl_));
}

void message::creation_time(amqp_timestamp t) {
    pn_message_set_creation_time(impl_, t);
}
amqp_timestamp message::creation_time() const {
    return pn_message_get_creation_time(impl_);
}

void message::group_id(const std::string &s) {
    check(pn_message_set_group_id(impl_, s.c_str()));
}

std::string message::group_id() const {
    const char* s = pn_message_get_group_id(impl_);
    return s ? std::string(s) : std::string();
}

void message::reply_to_group_id(const std::string &s) {
    check(pn_message_set_reply_to_group_id(impl_, s.c_str()));
}

std::string message::reply_to_group_id() const {
    const char* s = pn_message_get_reply_to_group_id(impl_);
    return s ? std::string(s) : std::string();
}

void message::body(const value& v) {
    set_value(pn_message_body(impl_), v);
}

void message::body(const values& v) {
    pn_data_copy(pn_message_body(impl_), v.data_);
}

const values& message::body() const {
    body_.view(pn_message_body(impl_));
    return body_;
}

values& message::body() {
    body_.view(pn_message_body(impl_));
    return body_;
}

void message::encode(std::string &s) const {
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

std::string message::encode() const {
    std::string data;
    encode(data);
    return data;
}

void message::decode(const std::string &s) {
    check(pn_message_decode(impl_, s.data(), s.size()));
}

pn_message_t *message::get() { return impl_; }

pn_message_t *message::release() {
    pn_message_t *result = impl_;
    impl_ = 0;
    return result;
}

void message::decode(proton::link link, proton::delivery delivery) {
    std::string buf;
    buf.resize(pn_delivery_pending(delivery.get()));
    ssize_t n = pn_link_recv(link.get(), (char *) buf.data(), buf.size());
    if (n != (ssize_t) buf.size()) throw error(MSG("link read failure"));
    clear();
    decode(buf);
    pn_link_advance(link.get());
}

}

