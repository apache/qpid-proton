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

#include "proton/delivery.hpp"
#include "proton/error.hpp"
#include "proton/link.hpp"
#include "proton/message.hpp"
#include "proton/message_id.hpp"
#include "proton/receiver.hpp"
#include "proton/sender.hpp"
#include "proton/timestamp.hpp"

#include "msg.hpp"
#include "proton_bits.hpp"
#include "types_internal.hpp"

#include "core/message-internal.h"
#include "proton/delivery.h"

#include <string>
#include <algorithm>
#include <assert.h>

namespace proton {

struct message::impl {
    value body;
    property_map properties;
    annotation_map annotations;
    annotation_map instructions;

    impl(pn_message_t *msg) {
        body.reset(pn_message_body(msg));
        properties.reset(pn_message_properties(msg));
        annotations.reset(pn_message_annotations(msg));
        instructions.reset(pn_message_instructions(msg));
    }

    void clear() {
        properties.clear();
        annotations.clear();
        instructions.clear();
    }

    // Encode cached maps to the pn_data_t, always used an empty() value for an empty map
    void flush() {
        if (!properties.empty()) properties.value();
        if (!annotations.empty()) annotations.value();
        if (!instructions.empty()) instructions.value();
    }
};

message::message() : pn_msg_(0) {}
message::message(const message &m) : pn_msg_(0) { *this = m; }

#if PN_CPP_HAS_RVALUE_REFERENCES
message::message(message &&m) : pn_msg_(0) { swap(*this, m); }
message& message::operator=(message&& m) {
  swap(*this, m);
  return *this;
}
#endif

message::message(const value& x) : pn_msg_(0) { body() = x; }

message::~message() {
    if (pn_msg_) {
        impl().~impl();               // destroy in-place
        pn_message_free(pn_msg_);
    }
}

void swap(message& x, message& y) {
    std::swap(x.pn_msg_, y.pn_msg_);
}

pn_message_t *message::pn_msg() const {
    if (!pn_msg_) {
        pn_msg_ = pni_message_with_extra(sizeof(struct message::impl));
        // Construct impl in extra storage allocated with pn_msg_
        new (pni_message_get_extra(pn_msg_)) struct message::impl(pn_msg_);
    }
    return pn_msg_;
}

struct message::impl& message::impl() const {
    return *(struct message::impl*)pni_message_get_extra(pn_msg());
}

message& message::operator=(const message& m) {
    if (&m != this) {
        // TODO aconway 2015-08-10: more efficient pn_message_copy function
        std::vector<char> data;
        m.encode(data);
        decode(data);
    }
    return *this;
}

void message::clear() {
    if (pn_msg_) {
        impl().clear();
        pn_message_clear(pn_msg_);
    }
}

namespace {
void check(int err) {
    if (err) throw error(error_str(err));
}
} // namespace

void message::id(const message_id& id) { pn_message_set_id(pn_msg(), id.atom_); }

message_id message::id() const {
    return pn_message_get_id(pn_msg());
}

void message::user(const std::string &id) {
    check(pn_message_set_user_id(pn_msg(), pn_bytes(id)));
}

std::string message::user() const {
    return str(pn_message_get_user_id(pn_msg()));
}

void message::to(const std::string &addr) {
    check(pn_message_set_address(pn_msg(), addr.c_str()));
}

std::string message::to() const {
    const char* addr = pn_message_get_address(pn_msg());
    return addr ? std::string(addr) : std::string();
}

void message::address(const std::string &addr) {
  check(pn_message_set_address(pn_msg(), addr.c_str()));
}

std::string message::address() const {
  const char* addr = pn_message_get_address(pn_msg());
  return addr ? std::string(addr) : std::string();
}

void message::subject(const std::string &s) {
    check(pn_message_set_subject(pn_msg(), s.c_str()));
}

std::string message::subject() const {
    const char* s = pn_message_get_subject(pn_msg());
    return s ? std::string(s) : std::string();
}

void message::reply_to(const std::string &s) {
    check(pn_message_set_reply_to(pn_msg(), s.c_str()));
}

std::string message::reply_to() const {
    const char* s = pn_message_get_reply_to(pn_msg());
    return s ? std::string(s) : std::string();
}

void message::correlation_id(const message_id& id) {
    value(pn_message_correlation_id(pn_msg())) = id;
}

message_id message::correlation_id() const {
    return pn_message_get_correlation_id(pn_msg());
}

void message::content_type(const std::string &s) {
    check(pn_message_set_content_type(pn_msg(), s.c_str()));
}

std::string message::content_type() const {
    const char* s = pn_message_get_content_type(pn_msg());
    return s ? std::string(s) : std::string();
}

void message::content_encoding(const std::string &s) {
    check(pn_message_set_content_encoding(pn_msg(), s.c_str()));
}

std::string message::content_encoding() const {
    const char* s = pn_message_get_content_encoding(pn_msg());
    return s ? std::string(s) : std::string();
}

void message::expiry_time(timestamp t) {
    pn_message_set_expiry_time(pn_msg(), t.milliseconds());
}
timestamp message::expiry_time() const {
    return timestamp(pn_message_get_expiry_time(pn_msg()));
}

void message::creation_time(timestamp t) {
    pn_message_set_creation_time(pn_msg(), t.milliseconds());
}
timestamp message::creation_time() const {
    return timestamp(pn_message_get_creation_time(pn_msg()));
}

void message::group_id(const std::string &s) {
    check(pn_message_set_group_id(pn_msg(), s.c_str()));
}

std::string message::group_id() const {
    const char* s = pn_message_get_group_id(pn_msg());
    return s ? std::string(s) : std::string();
}

void message::reply_to_group_id(const std::string &s) {
    check(pn_message_set_reply_to_group_id(pn_msg(), s.c_str()));
}

std::string message::reply_to_group_id() const {
    const char* s = pn_message_get_reply_to_group_id(pn_msg());
    return s ? std::string(s) : std::string();
}

bool message::inferred() const { return pn_message_is_inferred(pn_msg()); }

void message::inferred(bool b) { pn_message_set_inferred(pn_msg(), b); }

void message::body(const value& x) { body() = x; }

const value& message::body() const { return impl().body; }
value& message::body() { return impl().body; }

message::property_map& message::properties() {
    return impl().properties;
}

const message::property_map& message::properties() const {
    return impl().properties;
}

message::annotation_map& message::message_annotations() {
    return impl().annotations;
}

const message::annotation_map& message::message_annotations() const {
    return impl().annotations;
}

message::annotation_map& message::delivery_annotations() {
    return impl().instructions;
}

const message::annotation_map& message::delivery_annotations() const {
    return impl().instructions;
}

void message::encode(std::vector<char> &s) const {
    impl().flush();
    size_t sz = std::max(s.capacity(), size_t(512));
    while (true) {
        s.resize(sz);
        assert(!s.empty());
        int err = pn_message_encode(pn_msg(), const_cast<char*>(&s[0]), &sz);
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

std::vector<char> message::encode() const {
    std::vector<char> data;
    encode(data);
    return data;
}

void message::decode(const std::vector<char> &s) {
    if (s.empty())
        throw error("message decode: no data");
    impl().clear();
    check(pn_message_decode(pn_msg(), &s[0], s.size()));
}

bool message::durable() const { return pn_message_is_durable(pn_msg()); }
void message::durable(bool b) { pn_message_set_durable(pn_msg(), b); }

duration message::ttl() const { return duration(pn_message_get_ttl(pn_msg())); }
void message::ttl(duration d) { pn_message_set_ttl(pn_msg(), d.milliseconds()); }

uint8_t message::priority() const { return pn_message_get_priority(pn_msg()); }
void message::priority(uint8_t d) { pn_message_set_priority(pn_msg(), d); }

bool message::first_acquirer() const { return pn_message_is_first_acquirer(pn_msg()); }
void message::first_acquirer(bool b) { pn_message_set_first_acquirer(pn_msg(), b); }

uint32_t message::delivery_count() const { return pn_message_get_delivery_count(pn_msg()); }
void message::delivery_count(uint32_t d) { pn_message_set_delivery_count(pn_msg(), d); }

int32_t message::group_sequence() const { return pn_message_get_group_sequence(pn_msg()); }
void message::group_sequence(int32_t d) { pn_message_set_group_sequence(pn_msg(), d); }

const uint8_t message::default_priority = PN_DEFAULT_PRIORITY;

std::ostream& operator<<(std::ostream& o, const message& m) {
    return o << inspectable(m.pn_msg());
}

std::string to_string(const message& m) {
    std::ostringstream os;
    os << m;
    return os.str();
}

}
