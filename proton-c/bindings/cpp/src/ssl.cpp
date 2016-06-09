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
#include "proton/ssl.hpp"
#include "proton/error.hpp"
#include "msg.hpp"

#include <proton/ssl.h>

namespace proton {

std::string ssl::cipher() const {
    char buf[128];
    if (pn_ssl_get_cipher_name(object_, buf, sizeof(buf)))
        return std::string(buf);
    return std::string();
}

int ssl::ssf() const {
    return pn_ssl_get_ssf(object_);
}

std::string ssl::protocol() const {
    char buf[128];
    if (pn_ssl_get_protocol_name(object_, buf, sizeof(buf)))
        return std::string(buf);
    return std::string();
}

enum ssl::resume_status ssl::resume_status() const {
    return (enum ssl::resume_status)pn_ssl_resume_status(object_);
}

std::string ssl::remote_subject() const {
    const char *s = pn_ssl_get_remote_subject(object_);
    return s ? std::string(s) : std::string();
}


} // namespace
