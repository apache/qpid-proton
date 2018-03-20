#ifndef PROTON_SSL_HPP
#define PROTON_SSL_HPP

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

#include "./internal/export.hpp"
#include "./internal/config.hpp"


#include <proton/ssl.h>

#include <string>

/// @file
/// @copybrief proton::ssl

namespace proton {

/// SSL information.
class ssl {
    /// @cond INTERNAL
    ssl(pn_ssl_t* s) : object_(s) {}
    /// @endcond

#if PN_CPP_HAS_DELETED_FUNCTIONS
    ssl() = delete;
#else
    ssl();
#endif

  public:
    /// Determines the level of peer validation.
    enum verify_mode {
        /// Require peer to provide a valid identifying certificate
        VERIFY_PEER = PN_SSL_VERIFY_PEER,
        /// Do not require a certificate or cipher authorization
        ANONYMOUS_PEER = PN_SSL_ANONYMOUS_PEER,
        /// Require valid certificate and matching name
        VERIFY_PEER_NAME = PN_SSL_VERIFY_PEER_NAME
    };

    /// Outcome specifier for an attempted session resume.
    enum resume_status {
        UNKNOWN = PN_SSL_RESUME_UNKNOWN, ///< Session resume state unknown or not supported
        NEW = PN_SSL_RESUME_NEW,         ///< Session renegotiated, not resumed
        REUSED = PN_SSL_RESUME_REUSED    ///< Session resumed from previous session
    };

    /// @cond INTERNAL

    /// XXX C API uses cipher_name
    /// Get the cipher name.
    PN_CPP_EXTERN std::string cipher() const;

    /// XXX C API uses protocol_name
    /// Get the protocol name.
    PN_CPP_EXTERN std::string protocol() const;

    /// Get the security strength factor.
    PN_CPP_EXTERN int ssf() const;

    /// XXX discuss, what's the meaning of "remote" here?
    PN_CPP_EXTERN std::string remote_subject() const;

    /// XXX setters? versus connection options
    PN_CPP_EXTERN void resume_session_id(const std::string& session_id);

    PN_CPP_EXTERN enum resume_status resume_status() const;

    /// @endcond

  private:
    pn_ssl_t* const object_;

    /// @cond INTERNAL
  friend class transport;
    /// @endcond
};

/// **Unsettled API** - An SSL certificate.
class ssl_certificate {
  public:
    /// Create an SSL certificate.
    PN_CPP_EXTERN ssl_certificate(const std::string &certdb_main);

    // XXX Document the following constructors

    /// @copydoc ssl_certificate
    PN_CPP_EXTERN ssl_certificate(const std::string &certdb_main, const std::string &certdb_extra);

    /// @copydoc ssl_certificate
    PN_CPP_EXTERN ssl_certificate(const std::string &certdb_main, const std::string &certdb_extra, const std::string &passwd);
    /// @endcond

  private:
    std::string certdb_main_;
    std::string certdb_extra_;
    std::string passwd_;
    bool pw_set_;

    /// @cond INTERNAL
  friend class ssl_client_options;
  friend class ssl_server_options;
    /// @endcond
};

class ssl_domain_impl;

namespace internal {

// Base class for SSL configuration
class ssl_domain {
  public:
    PN_CPP_EXTERN ssl_domain(const ssl_domain&);
    PN_CPP_EXTERN ssl_domain& operator=(const ssl_domain&);
    PN_CPP_EXTERN ~ssl_domain();

  protected:
    ssl_domain(bool is_server);
    pn_ssl_domain_t *pn_domain();

  private:
    ssl_domain_impl *impl_;
    bool server_type_;
};

}

/// **Unsettled API** - SSL configuration for inbound connections.
class ssl_server_options : private internal::ssl_domain {
  public:
    /// Server SSL options based on the supplied X.509 certificate
    /// specifier.
    PN_CPP_EXTERN ssl_server_options(ssl_certificate &cert);

    /// Server SSL options requiring connecting clients to provide a
    /// client certificate.
    PN_CPP_EXTERN ssl_server_options(ssl_certificate &cert, const std::string &trust_db,
                                     const std::string &advertise_db = std::string(),
                                     enum ssl::verify_mode mode = ssl::VERIFY_PEER);

    /// Server SSL options restricted to available anonymous cipher
    /// suites on the platform.
    PN_CPP_EXTERN ssl_server_options();

  private:
    // Bring pn_domain into scope and allow connection_options to use
    // it.
    using internal::ssl_domain::pn_domain;

    /// @cond INTERNAL
  friend class connection_options;
    /// @endcond
};

/// **Unsettled API** - SSL configuration for outbound connections.
class ssl_client_options : private internal::ssl_domain {
  public:
    /// Create SSL client options (no client certificate).
    PN_CPP_EXTERN ssl_client_options(const std::string &trust_db,
                                     enum ssl::verify_mode = ssl::VERIFY_PEER_NAME);

    /// Create SSL client options with a client certificate.
    PN_CPP_EXTERN ssl_client_options(ssl_certificate&, const std::string &trust_db,
                                     enum ssl::verify_mode = ssl::VERIFY_PEER_NAME);

    /// SSL connections restricted to available anonymous cipher
    /// suites on the platform.
    PN_CPP_EXTERN ssl_client_options();

  private:
    // Bring pn_domain into scope and allow connection_options to use
    // it.
    using internal::ssl_domain::pn_domain;

    /// @cond INTERNAL
  friend class connection_options;
    /// @endcond
};

} // proton

#endif // PROTON_SSL_HPP
