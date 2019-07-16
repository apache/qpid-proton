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

#include "ssl_options_impl.hpp"

#include "proton/ssl.hpp"
#include "proton/error.hpp"
#include "msg.hpp"

#include <proton/ssl.h>

namespace {

void inline set_cred(pn_ssl_domain_t *dom, const std::string &main, const std::string &extra, const std::string &pass, bool pwset) {
    const char *cred2 = extra.empty() ? NULL : extra.c_str();
    const char *pw = pwset ? pass.c_str() : NULL;
    if (pn_ssl_domain_set_credentials(dom, main.c_str(), cred2, pw))
        throw proton::error(MSG("SSL certificate initialization failure for " << main << ":" <<
                        (cred2 ? cred2 : "NULL") << ":" << (pw ? pw : "NULL")));
}

void inline set_trusted_ca_db(pn_ssl_domain_t *dom, const std::string &trust_db) {
    if (pn_ssl_domain_set_trusted_ca_db(dom, trust_db.c_str()))
        throw proton::error(MSG("SSL trust store initialization failure for " << trust_db));
}

void inline set_client_verify_mode(pn_ssl_domain_t *dom, enum proton::ssl::verify_mode mode) {
    if (pn_ssl_domain_set_peer_authentication(dom, pn_ssl_verify_mode_t(mode), NULL))
        throw proton::error(MSG("SSL client verify mode failure"));
}

}

namespace proton {

ssl_options_impl::ssl_options_impl(bool is_server) :
    pn_domain_(pn_ssl_domain(is_server ? PN_SSL_MODE_SERVER : PN_SSL_MODE_CLIENT)),
    refcount_(1) {
    if (!pn_domain_) throw proton::error(MSG("SSL/TLS unavailable"));
}

ssl_options_impl::~ssl_options_impl() {
    pn_ssl_domain_free(pn_domain_);
}

ssl_server_options& ssl_server_options::operator=(const ssl_server_options& x) {
    if (&x!=this) {
        if (impl_) impl_->decref();
        impl_ = x.impl_;
        if (impl_) impl_->incref();
    }
    return *this;
}

ssl_server_options::ssl_server_options(const ssl_server_options& x): impl_(x.impl_) {
    if (impl_) impl_->incref();
}

ssl_server_options::~ssl_server_options() {
    if (impl_) impl_->decref();
}

ssl_server_options::ssl_server_options(const ssl_certificate &cert) : impl_(new impl) {
    set_cred(impl_->pn_domain(), cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
}

ssl_server_options::ssl_server_options(
    const ssl_certificate &cert,
    const std::string &trust_db,
    const std::string &advertise_db,
    enum ssl::verify_mode mode) : impl_(new impl)
{
    pn_ssl_domain_t* dom = impl_->pn_domain();
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    set_trusted_ca_db(dom, trust_db.c_str());
    const std::string &db = advertise_db.empty() ? trust_db : advertise_db;
    if (pn_ssl_domain_set_peer_authentication(dom, pn_ssl_verify_mode_t(mode), db.c_str()))
        throw error(MSG("SSL server configuration failure requiring client certificates using " << db));
}

ssl_server_options::ssl_server_options() : impl_(new impl) {}

ssl_client_options::ssl_client_options(const ssl_client_options& x): impl_(x.impl_) {
    if (impl_) impl_->incref();
}

ssl_client_options& ssl_client_options::operator=(const ssl_client_options& x) {
    if (&x!=this) {
        if (impl_) impl_->decref();
        impl_ = x.impl_;
        if (impl_) impl_->incref();
    }
    return *this;
}

ssl_client_options::~ssl_client_options() {
    if (impl_) impl_->decref();
}

ssl_client_options::ssl_client_options() : impl_(0) {}

ssl_client_options::ssl_client_options(enum ssl::verify_mode mode) : impl_(new impl) {
    pn_ssl_domain_t* dom = impl_->pn_domain();
    set_client_verify_mode(dom, mode);
}

ssl_client_options::ssl_client_options(const std::string &trust_db, enum ssl::verify_mode mode) : impl_(new impl) {
    pn_ssl_domain_t* dom = impl_->pn_domain();
    set_trusted_ca_db(dom, trust_db);
    set_client_verify_mode(dom, mode);
}

ssl_client_options::ssl_client_options(const ssl_certificate &cert, const std::string &trust_db, enum ssl::verify_mode mode) : impl_(new impl) {
    pn_ssl_domain_t* dom = impl_->pn_domain();
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    set_trusted_ca_db(dom, trust_db);
    set_client_verify_mode(dom, mode);
}

ssl_certificate::ssl_certificate(const std::string &main)
    : certdb_main_(main), pw_set_(false) {}

ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra)
    : certdb_main_(main), certdb_extra_(extra), pw_set_(false) {}

ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra, const std::string &pw)
    : certdb_main_(main), certdb_extra_(extra), passwd_(pw), pw_set_(true) {}

} // namespace
