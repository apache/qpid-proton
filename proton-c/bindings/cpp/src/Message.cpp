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

namespace proton {
namespace reactor {

Message::Message() : pnMessage(pn_message()){}

Message::~Message() {
    pn_decref(pnMessage);
}

Message::Message(const Message& m) : pnMessage(m.pnMessage) {
    pn_incref(pnMessage);
}

Message& Message::operator=(const Message& m) {
    pnMessage = m.pnMessage;
    pn_incref(pnMessage);
    return *this;
}

void Message::setBody(const std::string &buf) {
    pn_data_t *body = pn_message_body(pnMessage);
    pn_data_put_string(body, pn_bytes(buf.size(), buf.data()));
}

std::string Message::getBody() {
    pn_data_t *body = pn_message_body(pnMessage);
    if (pn_data_next(body) && pn_data_type(body) == PN_STRING) {
        pn_bytes_t bytes= pn_data_get_string(body);
        if (!pn_data_next(body)) {
            // String data and nothing else
            return std::string(bytes.start, bytes.size);
        }
    }

    pn_data_rewind(body);
    std::string str;
    size_t sz = 1024;
    str.resize(sz);
    int err = pn_data_format(body, (char *) str.data(), &sz);
    if (err == PN_OVERFLOW)
        throw ProtonException(MSG("TODO: sizing loop missing"));
    if (err) throw ProtonException(MSG("Unexpected data error"));
    str.resize(sz);
    return str;
}

void Message::encode(std::string &s) {
    size_t sz = 1024;
    if (s.capacity() > sz)
        sz = s.capacity();
    else
        s.reserve(sz);
    s.resize(sz);
    int err = pn_message_encode(pnMessage, (char *) s.data(), &sz);
    if (err == PN_OVERFLOW)
        throw ProtonException(MSG("TODO: fix overflow with dynamic buffer resizing"));
    if (err) throw ProtonException(MSG("unexpected error"));
    s.resize(sz);
}

void Message::decode(const std::string &s) {
    int err = pn_message_decode(pnMessage, s.data(), s.size());
    if (err) throw ProtonException(MSG("unexpected error"));
}


}} // namespace proton::reactor
