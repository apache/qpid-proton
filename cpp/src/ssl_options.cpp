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

// https://stackoverflow.com/questions/31657499/how-to-detect-stdlib-libc-in-the-preprocessor
#include <ciso646>

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

ssl_server_options::ssl_server_options() : impl_(0) {}

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

// Hack alert - don't do this at home - just design better initially!
// For backwards ABI compatibility we need to define some symbols:
//

// Don't do this with libc++ or with Visual Studio
#if !defined(_LIBCPP_VERSION) && !defined(_MSC_VER)

//
// These are a bit easier as the entire class has been removed so we can just define the class here
// They do rely on the impl_ member being effectively the same type and structure location from
// The previous parent type ssl_domain to ssl_server_options/ssl_client_options.
//
// PN_CPP_EXTERN proton::internal::ssl_domain::ssl_domain(const ssl_domain&);
// PN_CPP_EXTERN proton::internal::ssl_domain::ssl_domain& operator=(const ssl_domain&);
// PN_CPP_EXTERN proton::internal::ssl_domain::~ssl_domain();
//
namespace proton {
namespace internal {
class ssl_domain {
  PN_CPP_EXTERN ssl_domain(const ssl_domain&);
  PN_CPP_EXTERN ssl_domain& operator=(const ssl_domain&);
  PN_CPP_EXTERN ~ssl_domain();

  ssl_options_impl* impl_;
};

ssl_domain::ssl_domain(const ssl_domain& x): impl_(x.impl_) { if (impl_) impl_->incref(); }
ssl_domain& ssl_domain::operator=(const ssl_domain& x) {
    if (&x != this) {
        if (impl_) impl_->decref();
        impl_ = x.impl_;
        if (impl_) impl_->incref();
    }
    return *this;
}
ssl_domain::~ssl_domain() { if (impl_) impl_->decref(); }
}}

// These are a bit harder because they can't be publicly in the classes anymore so just use alias definitions
// but that needs the mangled symbol names unfortunately.
//
// PN_CPP_EXTERN proton::ssl_server_options::ssl_server_options(ssl_certificate &cert);
// PN_CPP_EXTERN proton::ssl_server_options::ssl_server_options(ssl_certificate &cert, const std::string &trust_db,
//                                                              const std::string &advertise_db = std::string(),
//                                                              enum ssl::verify_mode mode = ssl::VERIFY_PEER);
// PN_CPP_EXTERN proton::ssl_client_options::ssl_client_options(ssl_certificate&, const std::string &trust_db,
//                                                              enum ssl::verify_mode = ssl::VERIFY_PEER_NAME);

extern "C" {
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wattribute-alias"
// 2 variants of each constructor (base & complete)
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC1ERNS_15ssl_certificateE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC1ERKNS_15ssl_certificateE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC2ERNS_15ssl_certificateE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC2ERKNS_15ssl_certificateE")));
#if defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI>0
// Post gcc 5.1 "C++11" ABI
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC1ERNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESA_NS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC1ERKNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESB_NS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC2ERNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESA_NS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC2ERKNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESB_NS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_client_optionsC1ERNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_client_optionsC1ERKNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_client_optionsC2ERNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_client_optionsC2ERKNS_15ssl_certificateERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_3ssl11verify_modeE")));
#else
// Pre gcc 5.1 ABI
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC1ERNS_15ssl_certificateERKSsS4_NS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC1ERKNS_15ssl_certificateERKSsS5_NS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_server_optionsC2ERNS_15ssl_certificateERKSsS4_NS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_server_optionsC2ERKNS_15ssl_certificateERKSsS5_NS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_client_optionsC1ERNS_15ssl_certificateERKSsNS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_client_optionsC1ERKNS_15ssl_certificateERKSsNS_3ssl11verify_modeE")));
PN_CPP_EXTERN void    _ZN6proton18ssl_client_optionsC2ERNS_15ssl_certificateERKSsNS_3ssl11verify_modeE()
__attribute__((alias("_ZN6proton18ssl_client_optionsC2ERKNS_15ssl_certificateERKSsNS_3ssl11verify_modeE")));
#endif
}

#endif
