#ifndef PROTON_CONNECTION_OPTIONS_H
#define PROTON_CONNECTION_OPTIONS_H

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

#include "./duration.hpp"
#include "./fwd.hpp"
#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"
#include "./symbol.hpp"
#include "./types_fwd.hpp"

#include <proton/type_compat.h>

#include <map>
#include <vector>
#include <string>

/// @file
/// @copybrief proton::connection_options

struct pn_connection_t;
struct pn_transport_t;

namespace proton {

/// Options for creating a connection.
///
/// Options can be "chained" like this:
///
/// @code
/// c = container.connect(url, connection_options().handler(h).max_frame_size(1234));
/// @endcode
///
/// You can also create an options object with common settings and use
/// it as a base for different connections that have mostly the same
/// settings:
///
/// @code
/// connection_options opts;
/// opts.idle_timeout(1000).max_frame_size(10000);
/// c1 = container.connect(url1, opts.handler(h1));
/// c2 = container.connect(url2, opts.handler(h2));
/// @endcode
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class connection_options {
  public:
    /// Create an empty set of options.
    PN_CPP_EXTERN connection_options();

    /// Shorthand for `connection_options().handler(h)`.
    PN_CPP_EXTERN connection_options(class messaging_handler& h);

    /// Copy options.
    PN_CPP_EXTERN connection_options(const connection_options&);

    PN_CPP_EXTERN ~connection_options();

    /// Copy options.
    PN_CPP_EXTERN connection_options& operator=(const connection_options&);

    // XXX add C++11 move operations - Still relevant, and applies to all options

    /// Set a connection handler.
    ///
    /// The handler must not be deleted until
    /// messaging_handler::on_transport_close() is called.
    PN_CPP_EXTERN connection_options& handler(class messaging_handler&);

    /// Set the maximum frame size.  It is unlimited by default.
    PN_CPP_EXTERN connection_options& max_frame_size(uint32_t max);

    /// Set the maximum number of open sessions.  The default is 32767.
    PN_CPP_EXTERN connection_options& max_sessions(uint16_t max);

    /// Set the idle timeout.  The default is no timeout.
    ///
    /// If set, the local peer will disconnect if it receives no AMQP
    /// frames for an interval longer than `duration`.  Also known as
    /// "heartbeating", this is a way to detect dead peers even in the
    /// presence of a live TCP connection.
    PN_CPP_EXTERN connection_options& idle_timeout(duration);

    /// Set the container ID.
    PN_CPP_EXTERN connection_options& container_id(const std::string& id);

    /// Set the virtual host name for the connection. For client
    /// connections, it defaults to the host name used to set up the
    /// connection.  For server connections, it is unset by default.
    ///
    /// If making a client connection by SSL/TLS, this name is also
    /// used for certificate verification and Server Name Indication.
    PN_CPP_EXTERN connection_options& virtual_host(const std::string& name);

    /// Set the user name used to authenticate the connection.  It is
    /// unset by default.
    ///
    /// This value overrides any user name that is specified in the
    /// URL used for `container::connect`.  It is ignored if the
    /// connection is created by `container::listen` because a
    /// listening connection's identity is provided by the remote
    /// client.
    PN_CPP_EXTERN connection_options& user(const std::string&);

    /// Set the password used to authenticate the connection.
    ///
    /// This value is ignored if the connection is created by
    /// `container::listen`.
    PN_CPP_EXTERN connection_options& password(const std::string&);

    /// Set SSL client options.
    PN_CPP_EXTERN connection_options& ssl_client_options(const class ssl_client_options&);

    /// Set SSL server options.
    PN_CPP_EXTERN connection_options& ssl_server_options(const class ssl_server_options&);

    /// Enable or disable SASL.
    PN_CPP_EXTERN connection_options& sasl_enabled(bool);

    /// Force the enabling of SASL mechanisms that disclose cleartext
    /// passwords over the connection.  By default, such mechanisms
    /// are disabled.
    PN_CPP_EXTERN connection_options& sasl_allow_insecure_mechs(bool);

    /// Specify the SASL mechanisms allowed for this connection.
    /// The value is a space-separated list of mechanism names.
    ///
    /// The mechanisms allowed by default are determined by your SASL
    /// library and system configuration, with two exceptions: GSSAPI
    /// and GSS-SPNEGO are disabled by default.  To enable them, you
    /// must explicitly add them using this option.
    ///
    /// Clients must set the allowed mechanisms before the the
    /// outgoing connection is attempted.  Servers must set them
    /// before the listening connection is setup.
    PN_CPP_EXTERN connection_options& sasl_allowed_mechs(const std::string&);

    /// **Unsettled API** - Extension capabilities offered to the remote peer.
    PN_CPP_EXTERN connection_options& offered_capabilities(const std::vector<symbol>&);

    /// **Unsettled API** - Extension capabilities desired from the remote peer.
    PN_CPP_EXTERN connection_options& desired_capabilities(const std::vector<symbol>&);

    /// **Unsettled API** - Connection properties.
    PN_CPP_EXTERN connection_options& properties(const std::map<symbol, value>&);

    /// **Unsettled API** - Set the SASL configuration name.
    PN_CPP_EXTERN connection_options& sasl_config_name(const std::string&);

    /// **Unsettled API** - Set the SASL configuration path.
    PN_CPP_EXTERN connection_options& sasl_config_path(const std::string&);

    /// **Unsettled API** - Set reconnect timing options.
    ///
    /// If reconnect timing options are set, but no specific reconnect or failover urls are set
    /// then the original url used for the connection will be used as the reconnect url.
    PN_CPP_EXTERN connection_options& reconnect(const reconnect_options&);

    /// **Unsettled API** - Set reconnect URL.
    ///
    /// If the connection to the primary connection url fails then try to reconnect using
    /// the reconnect url.
    ///
    /// If this option is set then it will by default set the default reconnect timing options.
    ///
    /// If this option is changed using @ref connection::update_options before a reconnection
    /// attempt (for example in the @ref messaging_handler::on_transport_error callback) then
    /// the new value of the reconnect url will be used for the reconnect attempt rather then
    /// any previous value.
    ///
    /// If both the failover_urls and reconnect_url options are set then the behavior is not defined.
    PN_CPP_EXTERN connection_options& reconnect_url(const std::string&);

    /// **Unsettled API** - Set Fail-over URLs.
    ///
    /// If the connection to the primary connection url fails then try each of the fail-over
    /// connection urls in turn.
    ///
    /// If both the failover_urls and reconnect_url options are set then the behavior is not defined.
    PN_CPP_EXTERN connection_options& failover_urls(const std::vector<std::string>&);

    /// Update option values from values set in other.
    PN_CPP_EXTERN connection_options& update(const connection_options& other);

  private:
    void apply_unbound(connection&) const;
    void apply_reconnect_urls(pn_connection_t* pnc) const;
    void apply_unbound_client(pn_transport_t*) const;
    void apply_unbound_server(pn_transport_t*) const;
    messaging_handler* handler() const;

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
  friend class io::connection_driver;
  friend class connection;
    /// @endcond
};

} // proton

#endif // PROTON_CONNECTION_OPTIONS_H
