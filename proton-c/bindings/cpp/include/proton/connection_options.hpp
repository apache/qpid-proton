#ifndef PROTON_CPP_CONNECTION_OPTIONS_H
#define PROTON_CPP_CONNECTION_OPTIONS_H

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

#include <proton/config.hpp>
#include <proton/export.hpp>
#include <proton/duration.hpp>
#include <proton/pn_unique_ptr.hpp>
#include <proton/reconnect_timer.hpp>
#include <proton/types_fwd.hpp>


#include <vector>
#include <string>

struct pn_connection_t;

namespace proton {

class proton_handler;
class connection;

namespace io {
class connection_engine;
}

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

    /// Copy options.
    PN_CPP_EXTERN connection_options(const connection_options&);

    PN_CPP_EXTERN ~connection_options();

    /// Copy options.
    PN_CPP_EXTERN connection_options& operator=(const connection_options&);

    // XXX add C++11 move operations

    /// Set a handler for the connection.
    PN_CPP_EXTERN connection_options& handler(class handler *);

    /// Set the maximum frame size.
    PN_CPP_EXTERN connection_options& max_frame_size(uint32_t max);

    /// Set the maximum channels.
    PN_CPP_EXTERN connection_options& max_channels(uint16_t max);

    // XXX document relationship to heartbeat interval
    /// Set the idle timeout.
    PN_CPP_EXTERN connection_options& idle_timeout(duration);

    /// @cond INTERNAL
    /// XXX remove
    PN_CPP_EXTERN connection_options& heartbeat(duration);
    /// @endcond

    /// Set the container ID.
    PN_CPP_EXTERN connection_options& container_id(const std::string &id);

    /// @cond INTERNAL

    /// XXX more discussion - not clear we want to support this
    /// capability
    PN_CPP_EXTERN connection_options& link_prefix(const std::string &id);

    /// XXX settle questions about reconnect_timer - consider simply
    /// reconnect_options and making reconnect_timer internal
    PN_CPP_EXTERN connection_options& reconnect(const reconnect_timer &);

    /// @endcond

    /// Set SSL client options.
    PN_CPP_EXTERN connection_options& ssl_client_options(const class ssl_client_options &);

    /// Set SSL server options.
    PN_CPP_EXTERN connection_options& ssl_server_options(const class ssl_server_options &);

    /// Enable or disable SASL.
    PN_CPP_EXTERN connection_options& sasl_enabled(bool);

    /// Force the enabling of SASL mechanisms that disclose clear text
    /// passwords over the connection.  By default, such mechanisms
    /// are disabled.
    PN_CPP_EXTERN connection_options& sasl_allow_insecure_mechs(bool);

    /// Specify the allowed mechanisms for use on the connection.
    PN_CPP_EXTERN connection_options& sasl_allowed_mechs(const std::string &);

    /// Set the SASL configuration name.
    PN_CPP_EXTERN connection_options& sasl_config_name(const std::string &);

    /// @cond INTERNAL
    /// XXX not clear this should be exposed
    /// Set the SASL configuration path.
    PN_CPP_EXTERN connection_options& sasl_config_path(const std::string &);
    /// @endcond

    /// @cond INTERNAL
  private:
    void apply(connection&) const;
    proton_handler* handler() const;
    static pn_connection_t *pn_connection(connection &);
    class ssl_client_options &ssl_client_options();
    class ssl_server_options &ssl_server_options();
    PN_CPP_EXTERN void update(const connection_options& other);

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    friend class container_impl;
    friend class connector;
    friend class io::connection_engine;
    /// @endcond
};

}

#endif // PROTON_CPP_CONNECTION_OPTIONS_H
