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

#include "proton/url.hpp"

#include "proton/error.hpp"

#include <proton/url.h>

#include "proton_bits.hpp"

#include <sstream>

namespace proton {

url_error::url_error(const std::string& s) : error(s) {}

namespace {

pn_url_t* parse_throw(const char* s) {
    pn_url_t* u = pn_url_parse(s);
    if (!u) throw url_error("invalid URL: " + std::string(s));
    return u;
}

pn_url_t* parse_allow_empty(const char* s) {
    return s && *s ? parse_throw(s) : pn_url();
}

void replace(pn_url_t*& var, pn_url_t* val) {
    if (var) pn_url_free(var);
    var = val;
}

void defaults(pn_url_t* u) {
  const char* scheme = pn_url_get_scheme(u);
  const char* port = pn_url_get_port(u);
  if (!scheme || *scheme=='\0' ) pn_url_set_scheme(u, url::AMQP.c_str());
  if (!port || *port=='\0' ) pn_url_set_port(u, pn_url_get_scheme(u));
}

} // namespace

url::url(const std::string &s) : url_(parse_throw(s.c_str())) { defaults(url_); }

url::url(const std::string &s, bool d) : url_(parse_throw(s.c_str())) { if (d) defaults(url_); }

url::url(const url& u) : url_(parse_allow_empty(pn_url_str(u.url_))) {}

url::~url() { pn_url_free(url_); }

url& url::operator=(const url& u) {
    if (this != &u) replace(url_, parse_allow_empty(pn_url_str(u.url_)));
    return *this;
}

url::operator std::string() const { return str(pn_url_str(url_)); }

std::string url::scheme() const { return str(pn_url_get_scheme(url_)); }
std::string url::user() const { return str(pn_url_get_username(url_)); }
std::string url::password() const { return str(pn_url_get_password(url_)); }
std::string url::host() const { return str(pn_url_get_host(url_)); }
std::string url::port() const { return str(pn_url_get_port(url_)); }
std::string url::path() const { return str(pn_url_get_path(url_)); }

std::string url::host_port() const { return host() + ":" + port(); }

bool url::empty() const { return *pn_url_str(url_) == '\0'; }

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

std::ostream& operator<<(std::ostream& o, const url& u) {
    return o << pn_url_str(u.url_);
}

std::string to_string(const url& u) {
    return std::string(pn_url_str(u.url_));
}

std::istream& operator>>(std::istream& i, url& u) {
    std::string s;
    i >> s;
    if (!i.fail() && !i.bad()) {
        pn_url_t* p = pn_url_parse(s.c_str());
        if (p) {
            replace(u.url_, p);
            defaults(u.url_);
        } else {
            i.clear(std::ios::failbit);
        }
    }
    return i;
}

} // namespace proton
