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

ssl_domain::ssl_domain(bool server_type) {
    impl_ = pn_ssl_domain(server_type ? PN_SSL_MODE_SERVER : PN_SSL_MODE_CLIENT);
    if (!impl_) throw error(MSG("SSL/TLS unavailable"));
}

ssl_domain::~ssl_domain() { pn_ssl_domain_free(impl_); }

namespace {
void set_cred(pn_ssl_domain_t *dom, const std::string &main, const std::string &extra, const std::string &pass, bool pwset) {
    const char *cred2 = extra.empty() ? NULL : extra.c_str();
    const char *pw = pwset ? pass.c_str() : NULL;
    if (pn_ssl_domain_set_credentials(dom, main.c_str(), cred2, pw))
        throw error(MSG("SSL certificate initialization failure for " << main << ":" <<
                        (cred2 ? cred2 : "NULL") << ":" << (pw ? pw : "NULL")));
}
}

server_domain::server_domain(ssl_certificate &cert) :
    ssl_domain_(new ssl_domain(true)) {
    set_cred(ssl_domain_->impl_, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
}

server_domain::server_domain(ssl_certificate &cert, const std::string &trust_db, const std::string &advertise_db,
                             ssl::verify_mode_t mode) :
    ssl_domain_(new ssl_domain(true)) {
    pn_ssl_domain_t *dom = ssl_domain_->impl_;
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    if (pn_ssl_domain_set_trusted_ca_db(dom, trust_db.c_str()))
        throw error(MSG("SSL trust store initialization failure for " << trust_db));
    const std::string &db = advertise_db.empty() ? trust_db : advertise_db;
    if (pn_ssl_domain_set_peer_authentication(dom, (pn_ssl_verify_mode_t) mode, db.c_str()))
        throw error(MSG("SSL server configuration failure requiring client certificates using " << db));
}

// Keep default constructor low overhead for default use in connection_options.
server_domain::server_domain() : ssl_domain_(0) {}

pn_ssl_domain_t* server_domain::pn_domain() {
    if (!ssl_domain_) {
        // Lazily create anonymous domain context (no cert).  Could make it a singleton, but rare use?
        ssl_domain_.reset(new ssl_domain(true));
    }
    return ssl_domain_->impl_;
}

namespace {
void client_setup(pn_ssl_domain_t *dom, const std::string &trust_db, ssl::verify_mode_t mode) {
    if (pn_ssl_domain_set_trusted_ca_db(dom, trust_db.c_str()))
        throw error(MSG("SSL trust store initialization failure for " << trust_db));
    if (pn_ssl_domain_set_peer_authentication(dom, (pn_ssl_verify_mode_t) mode, NULL))
        throw error(MSG("SSL client verify mode failure"));
}
}

client_domain::client_domain(const std::string &trust_db, ssl::verify_mode_t mode) :
    ssl_domain_(new ssl_domain(false)) {
    client_setup(ssl_domain_->impl_, trust_db, mode);
}

client_domain::client_domain(ssl_certificate &cert, const std::string &trust_db, ssl::verify_mode_t mode) :
    ssl_domain_(new ssl_domain(false)) {
    pn_ssl_domain_t *dom = ssl_domain_->impl_;
    set_cred(dom, cert.certdb_main_, cert.certdb_extra_, cert.passwd_, cert.pw_set_);
    client_setup(dom, trust_db, mode);
}

client_domain::client_domain() : ssl_domain_(0) {}

pn_ssl_domain_t* client_domain::pn_domain() {
    if (!ssl_domain_) {
        // Lazily create anonymous domain context (no CA).  Could make it a singleton, but rare use?
        ssl_domain_.reset(new ssl_domain(false));
    }
    return ssl_domain_->impl_;
}


ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra)
    : certdb_main_(main), certdb_extra_(extra), pw_set_(false) {}

ssl_certificate::ssl_certificate(const std::string &main, const std::string &extra, const std::string &pw)
    : certdb_main_(main), certdb_extra_(extra), passwd_(pw), pw_set_(true) {}


} // namespace
