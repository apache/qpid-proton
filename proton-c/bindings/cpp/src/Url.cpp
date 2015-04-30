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

#include "proton/cpp/exceptions.h"
#include "Url.h"
#include "ProtonImplRef.h"
#include "Msg.h"

namespace proton {
namespace reactor {

template class ProtonHandle<pn_url_t>;
typedef ProtonImplRef<Url> PI;


Url::Url(const std::string &url) {
    pn_url_t *up = pn_url_parse(url.c_str());
    // refcount is 1, no need to incref
    if (!up)
        throw ProtonException(MSG("invalid URL: " << url));
    impl = up;
}

Url::~Url() { PI::dtor(*this); }

Url::Url(const Url& c) : ProtonHandle<pn_url_t>() {
    PI::copy(*this, c);
}

Url& Url::operator=(const Url& c) {
    return PI::assign(*this, c);
}

std::string Url::getPort() {
    const char *p = pn_url_get_port(impl);
    if (!p)
        return std::string("5672");
    else
        return std::string(p);
}

std::string Url::getHost() {
    const char *p = pn_url_get_host(impl);
    if (!p)
        return std::string("0.0.0.0");
    else
        return std::string(p);
}

std::string Url::getPath() {
    const char *p = pn_url_get_path(impl);
    if (!p)
        return std::string("");
    else
        return std::string(p);
}


}} // namespace proton::reactor
