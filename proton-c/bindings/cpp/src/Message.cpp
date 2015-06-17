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

#include "proton/Message.hpp"
#include "proton/Error.hpp"
#include "proton/message.h"
#include "Msg.hpp"
#include "proton_bits.hpp"
#include "ProtonImplRef.hpp"

#include <cstring>

namespace proton {

namespace reactor {
template class ProtonHandle<pn_message_t>;
}
typedef reactor::ProtonImplRef<Message> PI;

Message::Message() : body_(0) {
    PI::ctor(*this, 0);
}
Message::Message(pn_message_t *p) : body_(0) {
    PI::ctor(*this, p);
}
Message::Message(const Message& m) : ProtonHandle<pn_message_t>(), body_(0) {
    PI::copy(*this, m);
}

// FIXME aconway 2015-06-17: Message should be a value type, needs to own pn_message_t
// and do appropriate _copy and _free operations.
Message& Message::operator=(const Message& m) {
    return PI::assign(*this, m);
}
Message::~Message() { PI::dtor(*this); }

namespace {
void confirm(pn_message_t * const&  p) {
    if (p) return;
    const_cast<pn_message_t*&>(p) = pn_message(); // Correct refcount of 1
    if (!p)
        throw Error(MSG("No memory"));
}

void check(int err) {
    if (err) throw Error(errorStr(err));
}

void setValue(pn_data_t* d, const Value& v) {
    Values values(d);
    values.clear();
    values << v;
}

Value getValue(pn_data_t* d) {
    Values values(d);
    values.rewind();
    return values.get<Value>();
}
} // namespace

void Message::id(const Value& id) {
    confirm(impl);
    setValue(pn_message_id(impl), id);
}

Value Message::id() const {
    confirm(impl);
    return getValue(pn_message_id(impl));
}
void Message::user(const std::string &id) {
    confirm(impl);
    check(pn_message_set_user_id(impl, pn_bytes(id)));
}

std::string Message::user() const {
    confirm(impl);
    return str(pn_message_get_user_id(impl));
}

void Message::address(const std::string &addr) {
    confirm(impl);
    check(pn_message_set_address(impl, addr.c_str()));
}

std::string Message::address() const {
    confirm(impl);
    const char* addr = pn_message_get_address(impl);
    return addr ? std::string(addr) : std::string();
}

void Message::subject(const std::string &s) {
    confirm(impl);
    check(pn_message_set_subject(impl, s.c_str()));
}

std::string Message::subject() const {
    confirm(impl);
    const char* s = pn_message_get_subject(impl);
    return s ? std::string(s) : std::string();
}

void Message::replyTo(const std::string &s) {
    confirm(impl);
    check(pn_message_set_reply_to(impl, s.c_str()));
}

std::string Message::replyTo() const {
    confirm(impl);
    const char* s = pn_message_get_reply_to(impl);
    return s ? std::string(s) : std::string();
}

void Message::correlationId(const Value& id) {
    confirm(impl);
    setValue(pn_message_correlation_id(impl), id);
}

Value Message::correlationId() const {
    confirm(impl);
    return getValue(pn_message_correlation_id(impl));
}

void Message::contentType(const std::string &s) {
    confirm(impl);
    check(pn_message_set_content_type(impl, s.c_str()));
}

std::string Message::contentType() const {
    confirm(impl);
    const char* s = pn_message_get_content_type(impl);
    return s ? std::string(s) : std::string();
}

void Message::contentEncoding(const std::string &s) {
    confirm(impl);
    check(pn_message_set_content_encoding(impl, s.c_str()));
}

std::string Message::contentEncoding() const {
    confirm(impl);
    const char* s = pn_message_get_content_encoding(impl);
    return s ? std::string(s) : std::string();
}

void Message::expiry(Timestamp t) {
    confirm(impl);
    pn_message_set_expiry_time(impl, t.milliseconds);
}
Timestamp Message::expiry() const {
    confirm(impl);
    return Timestamp(pn_message_get_expiry_time(impl));
}

void Message::creationTime(Timestamp t) {
    confirm(impl);
    pn_message_set_creation_time(impl, t);
}
Timestamp Message::creationTime() const {
    confirm(impl);
    return pn_message_get_creation_time(impl);
}

void Message::groupId(const std::string &s) {
    confirm(impl);
    check(pn_message_set_group_id(impl, s.c_str()));
}

std::string Message::groupId() const {
    confirm(impl);
    const char* s = pn_message_get_group_id(impl);
    return s ? std::string(s) : std::string();
}

void Message::replyToGroupId(const std::string &s) {
    confirm(impl);
    check(pn_message_set_reply_to_group_id(impl, s.c_str()));
}

std::string Message::replyToGroupId() const {
    confirm(impl);
    const char* s = pn_message_get_reply_to_group_id(impl);
    return s ? std::string(s) : std::string();
}

void Message::body(const Value& v) {
    confirm(impl);
    setValue(pn_message_body(impl), v);
}

void Message::body(const Values& v) {
    confirm(impl);
    pn_data_copy(pn_message_body(impl), v.data);
}

const Values& Message::body() const {
    confirm(impl);
    body_.view(pn_message_body(impl));
    return body_;
}

Values& Message::body() {
    confirm(impl);
    body_.view(pn_message_body(impl));
    return body_;
}

void Message::encode(std::string &s) {
    confirm(impl);
    size_t sz = s.capacity();
    if (sz < 512) sz = 512;
    while (true) {
        s.resize(sz);
        int err = pn_message_encode(impl, (char *) s.data(), &sz);
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

void Message::decode(const std::string &s) {
    confirm(impl);
    check(pn_message_decode(impl, s.data(), s.size()));
}

pn_message_t *Message::pnMessage() const {
    return impl;
}

}
