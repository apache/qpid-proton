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
#include "url.hpp"
#include "proton_impl_ref.hpp"
#include "msg.hpp"

namespace proton {

template class proton_handle<pn_url_t>;
typedef proton_impl_ref<Url> PI;


Url::Url(const std::string &url) {
    pn_url_t *up = pn_url_parse(url.c_str());
    // refcount is 1, no need to incref
    if (!up)
        throw error(MSG("invalid URL: " << url));
    impl_ = up;
}

Url::~Url() { PI::dtor(*this); }

Url::Url(const Url& c) : proton_handle<pn_url_t>() {
    PI::copy(*this, c);
}

Url& Url::operator=(const Url& c) {
    return PI::assign(*this, c);
}

std::string Url::port() {
    const char *p = pn_url_get_port(impl_);
    if (!p)
        return std::string("5672");
    else
        return std::string(p);
}

std::string Url::host() {
    const char *p = pn_url_get_host(impl_);
    if (!p)
        return std::string("0.0.0.0");
    else
        return std::string(p);
}

std::string Url::path() {
    const char *p = pn_url_get_path(impl_);
    if (!p)
        return std::string("");
    else
        return std::string(p);
}


}
