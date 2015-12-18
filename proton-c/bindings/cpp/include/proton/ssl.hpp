#ifndef PROTON_CPP_SSL_H
#define PROTON_CPP_SSL_H

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
#include "proton/export.hpp"
#include "proton/ssl.h"

#include <string>

namespace proton {

class connection_options;

class ssl {
  public:
    enum verify_mode_t {
        VERIFY_PEER = PN_SSL_VERIFY_PEER,
        ANONYMOUS_PEER = PN_SSL_ANONYMOUS_PEER,
        VERIFY_PEER_NAME = PN_SSL_VERIFY_PEER_NAME
    };
    /// Outcome specifier for an attempted session resume
    enum resume_status_t {
        UNKNOWN = PN_SSL_RESUME_UNKNOWN,    /**< Session resume state unknown/not supported */
        NEW = PN_SSL_RESUME_NEW,            /**< Session renegotiated - not resumed */
        REUSED = PN_SSL_RESUME_REUSED       /**< Session resumed from previous session. */
    };
    ssl(pn_ssl_t* s) : object_(s) {}
    PN_CPP_EXTERN std::string cipher() const;
    PN_CPP_EXTERN std::string protocol() const;
    PN_CPP_EXTERN int ssf() const;
    PN_CPP_EXTERN void peer_hostname(const std::string &);
    PN_CPP_EXTERN std::string peer_hostname() const;
    PN_CPP_EXTERN std::string remote_subject() const;
    PN_CPP_EXTERN void resume_session_id(const std::string& session_id);
    PN_CPP_EXTERN resume_status_t resume_status() const;

private:
    pn_ssl_t* object_;
};


class ssl_certificate {
  public:
    PN_CPP_EXTERN ssl_certificate(const std::string &certdb_main, const std::string &certdb_extra = std::string());
    PN_CPP_EXTERN ssl_certificate(const std::string &certdb_main, const std::string &certdb_extra, const std::string &passwd);
  private:
    std::string certdb_main_;
    std::string certdb_extra_;
    std::string passwd_;
    bool pw_set_;
    friend class client_domain;
    friend class server_domain;
};


class ssl_domain_impl;

// Base class for SSL configuration
class ssl_domain {
  public:
    PN_CPP_EXTERN ssl_domain(const ssl_domain&);
    PN_CPP_EXTERN ssl_domain& operator=(const ssl_domain&);
    PN_CPP_EXTERN ~ssl_domain();

  protected:
    ssl_domain(bool is_server);
    pn_ssl_domain_t *pn_domain();
    void swap(ssl_domain &);

  private:
    ssl_domain_impl *impl_;
};

/** SSL/TLS configuration for inbound connections created from a listener */
class server_domain : private ssl_domain {
  public:
    /** A server domain based on the supplied X509 certificate specifier. */
    PN_CPP_EXTERN server_domain(ssl_certificate &cert);
    /** A server domain requiring connecting clients to provide a client certificate. */
    PN_CPP_EXTERN server_domain(ssl_certificate &cert, const std::string &trust_db,
                                const std::string &advertise_db = std::string(),
                                ssl::verify_mode_t mode = ssl::VERIFY_PEER);
    /** A server domain restricted to available anonymous cipher suites on the platform. */
    PN_CPP_EXTERN server_domain();

  private:
    // Bring pn_domain into scope and allow connection_options to use it
    using ssl_domain::pn_domain;
    friend class connection_options;
};


/** SSL/TLS configuration for outgoing connections created */
class client_domain : private ssl_domain {
  public:
    PN_CPP_EXTERN client_domain(const std::string &trust_db, ssl::verify_mode_t = ssl::VERIFY_PEER_NAME);
    PN_CPP_EXTERN client_domain(ssl_certificate&, const std::string &trust_db, ssl::verify_mode_t = ssl::VERIFY_PEER_NAME);
    /** A client domain restricted to available anonymous cipher suites on the platform. */
    PN_CPP_EXTERN client_domain();

  private:
    // Bring pn_domain into scope and allow connection_options to use it
    using ssl_domain::pn_domain;
    friend class connection_options;
};

}

#endif  /*!PROTON_CPP_SSL_H*/
