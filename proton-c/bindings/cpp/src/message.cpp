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

#include "proton/message.h"

#include "msg.hpp"
#include "proton_bits.hpp"
#include "types_internal.hpp"

#include <string>
#include <algorithm>
#include <assert.h>

namespace proton {

message::message() : pn_msg_(0) {}
message::message(const message &m) : pn_msg_(0) { *this = m; }

#if PN_CPP_HAS_CPP11
message::message(message &&m) : pn_msg_(0) { swap(*this, m); }
#endif

message::message(const value& x) : pn_msg_(0) { body() = x; }

message::~message() {
    body_.data_ = codec::data(0);      // Must release body before pn_message_free
    pn_message_free(pn_msg_);
}

void swap(message& x, message& y) {
    using std::swap;
    swap(x.pn_msg_, y.pn_msg_);
    swap(x.body_, y.body_);
    swap(x.application_properties_, y.application_properties_);
    swap(x.message_annotations_, y.message_annotations_);
    swap(x.delivery_annotations_, y.delivery_annotations_);
}

pn_message_t *message::pn_msg() const {
    if (!pn_msg_) pn_msg_ = pn_message();
    body_.data_ = pn_message_body(pn_msg_);
    return pn_msg_;
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

void message::clear() { if (pn_msg_) pn_message_clear(pn_msg_); }

namespace {
void check(int err) {
    if (err) throw error(error_str(err));
}
} // namespace

void message::id(const message_id& id) { pn_message_set_id(pn_msg(), id.scalar_.atom_); }

message_id message::id() const {
    return pn_message_get_id(pn_msg());
}

void message::user_id(const std::string &id) {
    check(pn_message_set_user_id(pn_msg(), pn_bytes(id)));
}

std::string message::user_id() const {
    return str(pn_message_get_user_id(pn_msg()));
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
    codec::encoder e(pn_message_correlation_id(pn_msg()));
    e << id.scalar_;
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
    pn_message_set_expiry_time(pn_msg(), t.ms());
}
timestamp message::expiry_time() const {
    return timestamp(pn_message_get_expiry_time(pn_msg()));
}

void message::creation_time(timestamp t) {
    pn_message_set_creation_time(pn_msg(), t.ms());
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

const value& message::body() const { pn_msg(); return body_; }
value& message::body() { pn_msg(); return body_; }

// MAP CACHING: the properties and annotations maps can either be encoded in the
// pn_message pn_data_t structures OR decoded as C++ map members of the message
// but not both. At least one of the pn_data_t or the map member is always
// empty, the non-empty one is the authority.

// Decode a map on demand
template<class M> M& get_map(pn_message_t* msg, pn_data_t* (*get)(pn_message_t*), M& map) {
    codec::decoder d(get(msg));
    if (map.empty() && !d.empty()) {
        d.rewind();
        d >> map;
        d.clear();              // The map member is now the authority.
    }
    return map;
}

// Encode a map if necessary.
template<class M> M& put_map(pn_message_t* msg, pn_data_t* (*get)(pn_message_t*), M& map) {
    codec::encoder e(get(msg));
    if (e.empty() && !map.empty()) {
        e << map;
        map.clear();            // The encoded pn_data_t  is now the authority.
    }
    return map;
}

message::property_map& message::application_properties() {
    return get_map(pn_msg(), pn_message_properties, application_properties_);
}

const message::property_map& message::application_properties() const {
    return get_map(pn_msg(), pn_message_properties, application_properties_);
}


message::annotation_map& message::message_annotations() {
    return get_map(pn_msg(), pn_message_annotations, message_annotations_);
}

const message::annotation_map& message::message_annotations() const {
    return get_map(pn_msg(), pn_message_annotations, message_annotations_);
}


message::annotation_map& message::delivery_annotations() {
    return get_map(pn_msg(), pn_message_instructions, delivery_annotations_);
}

const message::annotation_map& message::delivery_annotations() const {
    return get_map(pn_msg(), pn_message_instructions, delivery_annotations_);
}

void message::encode(std::vector<char> &s) const {
    put_map(pn_msg(), pn_message_properties, application_properties_);
    put_map(pn_msg(), pn_message_annotations, message_annotations_);
    put_map(pn_msg(), pn_message_instructions, delivery_annotations_);
    size_t sz = std::max(s.capacity(), size_t(512));
    while (true) {
        s.resize(sz);
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
    application_properties_.clear();
    message_annotations_.clear();
    delivery_annotations_.clear();
    check(pn_message_decode(pn_msg(), &s[0], s.size()));
}

void message::decode(proton::delivery delivery) {
    std::vector<char> buf;
    buf.resize(delivery.pending());
    proton::link link = delivery.link();
    ssize_t n = link.recv(const_cast<char *>(&buf[0]), buf.size());
    if (n != ssize_t(buf.size())) throw error(MSG("link read failure"));
    clear();
    decode(buf);
    link.advance();
}

bool message::durable() const { return pn_message_is_durable(pn_msg()); }
void message::durable(bool b) { pn_message_set_durable(pn_msg(), b); }

duration message::ttl() const { return duration(pn_message_get_ttl(pn_msg())); }
void message::ttl(duration d) { pn_message_set_ttl(pn_msg(), d.ms()); }

uint8_t message::priority() const { return pn_message_get_priority(pn_msg()); }
void message::priority(uint8_t d) { pn_message_set_priority(pn_msg(), d); }

bool message::first_acquirer() const { return pn_message_is_first_acquirer(pn_msg()); }
void message::first_acquirer(bool b) { pn_message_set_first_acquirer(pn_msg(), b); }

uint32_t message::delivery_count() const { return pn_message_get_delivery_count(pn_msg()); }
void message::delivery_count(uint32_t d) { pn_message_set_delivery_count(pn_msg(), d); }

int32_t message::group_sequence() const { return pn_message_get_group_sequence(pn_msg()); }
void message::group_sequence(int32_t d) { pn_message_set_group_sequence(pn_msg(), d); }

}
