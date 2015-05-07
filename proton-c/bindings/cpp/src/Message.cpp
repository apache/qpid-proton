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

#include "proton/cpp/Message.h"
#include "proton/cpp/exceptions.h"
#include "Msg.h"
#include "ProtonImplRef.h"

#include <cstring>

namespace proton {
namespace reactor {

template class ProtonHandle<pn_message_t>;
typedef ProtonImplRef<Message> PI;

Message::Message() {
    PI::ctor(*this, 0);
}
Message::Message(pn_message_t *p) {
    PI::ctor(*this, p);
}
Message::Message(const Message& m) : ProtonHandle<pn_message_t>() {
    PI::copy(*this, m);
}
Message& Message::operator=(const Message& m) {
    return PI::assign(*this, m);
}
Message::~Message() { PI::dtor(*this); }

namespace {
void confirm(pn_message_t *&p) {
    if (p) return;
    p = pn_message(); // Correct refcount of 1
    if (!p)
        throw ProtonException(MSG("No memory"));
}

void getFormatedStringContent(pn_data_t *data, std::string &str) {
    pn_data_rewind(data);
    size_t sz = str.capacity();
    if (sz < 512) sz = 512;
    while (true) {
        str.resize(sz);
        int err = pn_data_format(data, (char *) str.data(), &sz);
        if (err) {
            if (err != PN_OVERFLOW)
                throw ProtonException(MSG("Unexpected message body data error"));
        }
        else {
            str.resize(sz);
            return;
        }
        sz *= 2;
    }
}

} // namespace

void Message::setId(uint64_t id) {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_ulong(data, id))
        throw ProtonException(MSG("setId error " << err));
}

uint64_t Message::getId() {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_ULONG) {
        return pn_data_get_ulong(data);
    }
    throw ProtonException(MSG("Message ID is not a ULONG"));
}

void Message::setId(const std::string &id) {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_string(data, pn_bytes(id.size(), id.data())))
        throw ProtonException(MSG("setId error " << err));
}

std::string Message::getStringId() {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_STRING) {
        pn_bytes_t bytes = pn_data_get_string(data);
        return (std::string(bytes.start, bytes.size));
    }
    throw ProtonException(MSG("Message ID is not a string value"));
}

void Message::setId(const char *p, size_t len) {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_binary(data, pn_bytes(len, p)))
        throw ProtonException(MSG("setId error " << err));
}

size_t Message::getId(const char **p) {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_BINARY) {
        pn_bytes_t pnb = pn_data_get_binary(data);
        *p = pnb.start;
        return pnb.size;
    }
    throw ProtonException(MSG("Message ID is not a binary value"));
}

pn_type_t Message::getIdType() {
    confirm(impl);
    pn_data_t *data = pn_message_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data)) {
        pn_type_t type = pn_data_type(data);
        switch (type) {
        case PN_ULONG:
        case PN_STRING:
        case PN_BINARY:
        case PN_UUID:
            return type;
            break;
        default:
            break;
        }
    }
    return PN_NULL;
}

void Message::setUserId(const std::string &id) {
    confirm(impl);
    if (int err = pn_message_set_user_id(impl, pn_bytes(id.size(), id.data())))
        throw ProtonException(MSG("setUserId error " << err));
}

std::string Message::getUserId() {
    confirm(impl);
    pn_bytes_t bytes = pn_message_get_user_id(impl);
    return (std::string(bytes.start, bytes.size));
}

void Message::setAddress(const std::string &addr) {
    confirm(impl);
    if (int err = pn_message_set_address(impl, addr.c_str()))
        throw ProtonException(MSG("setAddress error " << err));
}

std::string Message::getAddress() {
    confirm(impl);
    const char* addr = pn_message_get_address(impl);
    return addr ? std::string(addr) : std::string();
}

void Message::setSubject(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_subject(impl, s.c_str()))
        throw ProtonException(MSG("setSubject error " << err));
}

std::string Message::getSubject() {
    confirm(impl);
    const char* s = pn_message_get_subject(impl);
    return s ? std::string(s) : std::string();
}

void Message::setReplyTo(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_reply_to(impl, s.c_str()))
        throw ProtonException(MSG("setReplyTo error " << err));
}

std::string Message::getReplyTo() {
    confirm(impl);
    const char* s = pn_message_get_reply_to(impl);
    return s ? std::string(s) : std::string();
}

void Message::setCorrelationId(uint64_t id) {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_ulong(data, id))
        throw ProtonException(MSG("setCorrelationId error " << err));
}

uint64_t Message::getCorrelationId() {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_ULONG) {
        return pn_data_get_ulong(data);
    }
    throw ProtonException(MSG("Correlation ID is not a ULONG"));
}

void Message::setCorrelationId(const std::string &id) {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_string(data, pn_bytes(id.size(), id.data())))
        throw ProtonException(MSG("setCorrelationId error " << err));
}

std::string Message::getStringCorrelationId() {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_STRING) {
        pn_bytes_t bytes = pn_data_get_string(data);
        return (std::string(bytes.start, bytes.size));
    }
    throw ProtonException(MSG("Message ID is not a string value"));
}

void Message::setCorrelationId(const char *p, size_t len) {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_clear(data);
    if (int err = pn_data_put_binary(data, pn_bytes(len, p)))
        throw ProtonException(MSG("setCorrelationId error " << err));
}

size_t Message::getCorrelationId(const char **p) {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data) && pn_data_type(data) == PN_BINARY) {
        pn_bytes_t pnb = pn_data_get_binary(data);
        *p = pnb.start;
        return pnb.size;
    }
    throw ProtonException(MSG("Message ID is not a binary value"));
}

pn_type_t Message::getCorrelationIdType() {
    confirm(impl);
    pn_data_t *data = pn_message_correlation_id(impl);
    pn_data_rewind(data);
    if (pn_data_size(data) == 1 && pn_data_next(data)) {
        pn_type_t type = pn_data_type(data);
        switch (type) {
        case PN_ULONG:
        case PN_STRING:
        case PN_BINARY:
        case PN_UUID:
            return type;
            break;
        default:
            break;
        }
    }
    return PN_NULL;
}

void Message::setContentType(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_content_type(impl, s.c_str()))
        throw ProtonException(MSG("setContentType error " << err));
}

std::string Message::getContentType() {
    confirm(impl);
    const char* s = pn_message_get_content_type(impl);
    return s ? std::string(s) : std::string();
}

void Message::setContentEncoding(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_content_encoding(impl, s.c_str()))
        throw ProtonException(MSG("setContentEncoding error " << err));
}

std::string Message::getContentEncoding() {
    confirm(impl);
    const char* s = pn_message_get_content_encoding(impl);
    return s ? std::string(s) : std::string();
}

void Message::setExpiry(pn_timestamp_t t) {
    confirm(impl);
    pn_message_set_expiry_time(impl, t);
}
pn_timestamp_t Message::getExpiry() {
    confirm(impl);
    return pn_message_get_expiry_time(impl);
}

void Message::setCreationTime(pn_timestamp_t t) {
    confirm(impl);
    pn_message_set_creation_time(impl, t);
}
pn_timestamp_t Message::getCreationTime() {
    confirm(impl);
    return pn_message_get_creation_time(impl);
}


void Message::setGroupId(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_group_id(impl, s.c_str()))
        throw ProtonException(MSG("setGroupId error " << err));
}

std::string Message::getGroupId() {
    confirm(impl);
    const char* s = pn_message_get_group_id(impl);
    return s ? std::string(s) : std::string();
}

void Message::setReplyToGroupId(const std::string &s) {
    confirm(impl);
    if (int err = pn_message_set_reply_to_group_id(impl, s.c_str()))
        throw ProtonException(MSG("setReplyToGroupId error " << err));
}

std::string Message::getReplyToGroupId() {
    confirm(impl);
    const char* s = pn_message_get_reply_to_group_id(impl);
    return s ? std::string(s) : std::string();
}


void Message::setBody(const std::string &buf) {
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_clear(body);
    pn_data_put_string(body, pn_bytes(buf.size(), buf.data()));
}

void Message::getBody(std::string &str) {
    // User supplied string/buffer
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_rewind(body);

    if (pn_data_next(body) && pn_data_type(body) == PN_STRING) {
        pn_bytes_t bytes = pn_data_get_string(body);
        if (!pn_data_next(body)) {
            // String data and nothing else
            str.resize(bytes.size);
            memmove((void *) str.data(), bytes.start, bytes.size);
            return;
        }
    }

    getFormatedStringContent(body, str);
}

std::string Message::getBody() {
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_rewind(body);

    if (pn_data_next(body) && pn_data_type(body) == PN_STRING) {
        pn_bytes_t bytes= pn_data_get_string(body);
        if (!pn_data_next(body)) {
            // String data and nothing else
            return std::string(bytes.start, bytes.size);
        }
    }

    std::string str;
    getFormatedStringContent(body, str);
    return str;
}

void Message::setBody(const char *bytes, size_t len) {
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_clear(body);
    pn_data_put_binary(body, pn_bytes(len, bytes));
}

size_t Message::getBody(char *bytes, size_t len) {
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_rewind(body);
    if (pn_data_size(body) == 1 && pn_data_next(body) && pn_data_type(body) == PN_BINARY) {
        pn_bytes_t pnb = pn_data_get_binary(body);
        if (len >= pnb.size) {
            memmove(bytes, pnb.start, pnb.size);
            return pnb.size;
        }
        throw ProtonException(MSG("Binary buffer too small"));
    }
    throw ProtonException(MSG("Not simple binary data"));
}



size_t Message::getBinaryBodySize() {
    confirm(impl);
    pn_data_t *body = pn_message_body(impl);
    pn_data_rewind(body);
    if (pn_data_size(body) == 1 && pn_data_next(body) && pn_data_type(body) == PN_BINARY) {
        pn_bytes_t bytes = pn_data_get_binary(body);
        return bytes.size;
    }
    return 0;
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
                throw ProtonException(MSG("unexpected error"));
        } else {
            s.resize(sz);
            return;
        }
        sz *= 2;
    }
}

void Message::decode(const std::string &s) {
    confirm(impl);
    int err = pn_message_decode(impl, s.data(), s.size());
    if (err) throw ProtonException(MSG("unexpected error"));
}

pn_message_t *Message::getPnMessage() const {
    return impl;
}

}} // namespace proton::reactor
