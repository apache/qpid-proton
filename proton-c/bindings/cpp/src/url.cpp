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

#include "proton/error.hpp"
#include "proton/url.hpp"
#include "proton/url.h"
#include <sstream>

namespace proton {

url_error::url_error(const std::string& s) : error(s) {}

namespace {

pn_url_t* parse_throw(const std::string& s) {
    pn_url_t* u = pn_url_parse(s.c_str());
    if (!u) throw url_error("invalid URL: " + s);
    return u;
}

pn_url_t* parse_allow_empty(const std::string& s) {
    return s.empty() ? pn_url() : parse_throw(s);
}

std::string char_str(const char* s) { return s ? std::string(s) : std::string(); }

void replace(pn_url_t*& var, pn_url_t* val) {
    if (var) pn_url_free(var);
    var = val;
}

} // namespace

url::url() : url_(pn_url()) {}

url::url(const std::string &s, bool d) : url_(parse_throw(s)) { if (d) defaults(); }

url::url(const char *s, bool d) : url_(parse_throw(s)) { if (d) defaults(); }

url::url(const url& u) : url_(parse_allow_empty(u.str())) {}

url::~url() { pn_url_free(url_); }

url& url::operator=(const url& u) {
    if (this != &u) replace(url_, parse_allow_empty(u.str()));
    return *this;
}

void url::parse(const std::string& s) { replace(url_, parse_throw(s)); }

void url::parse(const char *s) { replace(url_, parse_throw(s)); }

std::string url::str() const { return char_str(pn_url_str(url_)); }

std::string url::scheme() const { return char_str(pn_url_get_scheme(url_)); }
std::string url::username() const { return char_str(pn_url_get_username(url_)); }
std::string url::password() const { return char_str(pn_url_get_password(url_)); }
std::string url::host() const { return char_str(pn_url_get_host(url_)); }
std::string url::port() const { return char_str(pn_url_get_port(url_)); }
std::string url::path() const { return char_str(pn_url_get_path(url_)); }

std::string url::host_port() const { return host() + ":" + port(); }

bool url::empty() const { return str().empty(); }

void url::scheme(const std::string& s) { pn_url_set_scheme(url_, s.c_str()); }
void url::username(const std::string& s) { pn_url_set_username(url_, s.c_str()); }
void url::password(const std::string& s) { pn_url_set_password(url_, s.c_str()); }
void url::host(const std::string& s) { pn_url_set_host(url_, s.c_str()); }
void url::port(const std::string& s) { pn_url_set_port(url_, s.c_str()); }
void url::path(const std::string& s) { pn_url_set_path(url_, s.c_str()); }

void url::defaults() {
    if (scheme().empty()) scheme(AMQP);
    if (port().empty()) port(scheme());
}

const std::string url::AMQP("amqp");
const std::string url::AMQPS("amqps");

uint16_t url::port_int() const {
    // TODO aconway 2015-10-27: full service name lookup
    if (port() == AMQP) return 5672;
    if (port() == AMQPS) return 5671;
    std::istringstream is(port());
    uint16_t result;
    is >> result;
    if (is.fail())
        throw url_error("invalid port '" + port() + "'");
    return result;
}

std::ostream& operator<<(std::ostream& o, const url& u) { return o << u.str(); }

std::istream& operator>>(std::istream& i, url& u) {
    std::string s;
    i >> s;
    if (!i.fail() && !i.bad()) {
        pn_url_t* p = pn_url_parse(s.c_str());
        if (p) {
            replace(u.url_, p);
            u.defaults();
        } else {
            i.clear(std::ios::failbit);
        }
    }
    return i;
}

} // namespace proton
