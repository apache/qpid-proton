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

#include "proton/ssl.h"

namespace proton {

class ssl_domain_impl {
  public:
    ssl_domain_impl(bool is_server) : refcount_(1), server_type_(is_server), pn_domain_(0) {}
    void incref() { refcount_++; }
    void decref() {
        if (--refcount_ == 0) {
            if (pn_domain_) pn_ssl_domain_free(pn_domain_);
            delete this;
        }
    }
    pn_ssl_domain_t *pn_domain();
  private:
    int refcount_;
    bool server_type_;
    pn_ssl_domain_t *pn_domain_;
    ssl_domain_impl(const ssl_domain_impl&);
    ssl_domain_impl& operator=(const ssl_domain_impl&);
};

pn_ssl_domain_t *ssl_domain_impl::pn_domain() {
    if (pn_domain_) return pn_domain_;
    // Lazily create in case never actually used or configured.
    pn_domain_ = pn_ssl_domain(server_type_ ? PN_SSL_MODE_SERVER : PN_SSL_MODE_CLIENT);
    if (!pn_domain_) throw error(MSG("SSL/TLS unavailable"));
    return pn_domain_;
}

namespace internal {
ssl_domain::ssl_domain(bool is_server) : impl_(new ssl_domain_impl(is_server)) {}

ssl_domain::ssl_domain(const ssl_domain &x) {
    impl_ = x.impl_;
    impl_->incref();
}

ssl_domain& internal::ssl_domain::operator=(const ssl_domain&x) {
    impl_->decref();
    impl_ = x.impl_;
    impl_->incref();
    return *this;
}

ssl_domain::~ssl_domain() { impl_->decref(); }

pn_ssl_domain_t *ssl_domain::pn_domain() { return impl_->pn_domain(); }

} // namespace internal

namespace {

void set_cred(pn_ssl_domain_t *dom, const std::string &main, const std::string &extra, const std::string &pass, bool pwset) {
    const char *cred2 = extra.empty() ? NULL : extra.c_str();
    const char *pw = pwset ? pass.c_str() : NULL;
    if (pn_ssl_domain_set_credentials(dom, main.c_str(), cred2, pw))
        throw error(MSG("SSL certificate initialization failure for " << main << ":" <<
                        (cred2 ? cred2 : "NULL") << ":" << (pw ? pw : "NULL")));
}
}

ssl_server_options::ssl_server_options(ssl_certificate &cert) : internal::ssl_domain(true) {
    set_cred(pn_domain(), cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
}

ssl_server_options::ssl_server_options(
    ssl_certificate &cert,
    const std::string &trust_db,
    const std::string &advertise_db,
    enum ssl::verify_mode mode) : internal::ssl_domain(true)
{
    pn_ssl_domain_t* dom = pn_domain();
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    if (pn_ssl_domain_set_trusted_ca_db(dom, trust_db.c_str()))
        throw error(MSG("SSL trust store initialization failure for " << trust_db));
    const std::string &db = advertise_db.empty() ? trust_db : advertise_db;
    if (pn_ssl_domain_set_peer_authentication(dom, pn_ssl_verify_mode_t(mode), db.c_str()))
        throw error(MSG("SSL server configuration failure requiring client certificates using " << db));
}

ssl_server_options::ssl_server_options() : ssl_domain(true) {}

namespace {
void client_setup(pn_ssl_domain_t *dom, const std::string &trust_db, enum ssl::verify_mode mode) {
    if (pn_ssl_domain_set_trusted_ca_db(dom, trust_db.c_str()))
        throw error(MSG("SSL trust store initialization failure for " << trust_db));
    if (pn_ssl_domain_set_peer_authentication(dom, pn_ssl_verify_mode_t(mode), NULL))
        throw error(MSG("SSL client verify mode failure"));
}
}

ssl_client_options::ssl_client_options(const std::string &trust_db, enum ssl::verify_mode mode) : ssl_domain(false) {
    client_setup(pn_domain(), trust_db, mode);
}

ssl_client_options::ssl_client_options(ssl_certificate &cert, const std::string &trust_db, enum ssl::verify_mode mode) : ssl_domain(false) {
    pn_ssl_domain_t *dom = pn_domain();
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    client_setup(dom, trust_db, mode);
}

ssl_client_options::ssl_client_options() : ssl_domain(false) {}

ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra)
    : certdb_main_(main), certdb_extra_(extra), pw_set_(false) {}

ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra, const std::string &pw)
    : certdb_main_(main), certdb_extra_(extra), passwd_(pw), pw_set_(true) {}

} // namespace
