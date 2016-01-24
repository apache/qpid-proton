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
#include "proton/config.hpp"
#include "proton/export.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/reconnect_timer.hpp"
#include "proton/types.hpp"

#include <vector>
#include <string>

namespace proton {

class proton_handler;
class connection;

/** Options for creating a connection.
 *
 * Options can be "chained" like this:
 *
 * c = container.connect(url, connection_options().handler(h).max_frame_size(1234));
 *
 * You can also create an options object with common settings and use it as a base
 * for different connections that have mostly the same settings:
 *
 * connection_options opts;
 * opts.idle_timeout(1000).max_frame_size(10000);
 * c1 = container.connect(url1, opts.handler(h1));
 * c2 = container.connect(url2, opts.handler(h2));
 *
 * Normal value semantics, copy or assign creates a separate copy of the options.
 */
class connection_options {
  public:
    PN_CPP_EXTERN connection_options();
    PN_CPP_EXTERN connection_options(const connection_options&);
    PN_CPP_EXTERN ~connection_options();
    PN_CPP_EXTERN connection_options& operator=(const connection_options&);

    /// Override with options from other.
    PN_CPP_EXTERN void override(const connection_options& other);

    // TODO: Document options

    PN_CPP_EXTERN connection_options& handler(class handler *);
    PN_CPP_EXTERN connection_options& max_frame_size(uint32_t max);
    PN_CPP_EXTERN connection_options& max_channels(uint16_t max);
    PN_CPP_EXTERN connection_options& idle_timeout(duration);
    PN_CPP_EXTERN connection_options& heartbeat(duration);
    PN_CPP_EXTERN connection_options& container_id(const std::string &id);
    PN_CPP_EXTERN connection_options& link_prefix(const std::string &id);
    PN_CPP_EXTERN connection_options& reconnect(const reconnect_timer &);
    PN_CPP_EXTERN connection_options& ssl_client_options(const class ssl_client_options &);
    PN_CPP_EXTERN connection_options& ssl_server_options(const class ssl_server_options &);
    PN_CPP_EXTERN connection_options& peer_hostname(const std::string &name);
    PN_CPP_EXTERN connection_options& resume_id(const std::string &id);
    PN_CPP_EXTERN connection_options& sasl_enabled(bool);
    PN_CPP_EXTERN connection_options& allow_insecure_mechs(bool);
    PN_CPP_EXTERN connection_options& allowed_mechs(const std::string &);
    PN_CPP_EXTERN connection_options& sasl_config_name(const std::string &);
    PN_CPP_EXTERN connection_options& sasl_config_path(const std::string &);

  private:
    void apply(connection&) const;
    proton_handler* handler() const;
    static pn_connection_t *pn_connection(connection &);
    class ssl_client_options &ssl_client_options();
    class ssl_server_options &ssl_server_options();

    class impl;
    pn_unique_ptr<impl> impl_;

  friend class container_impl;
  friend class connector;
  friend class connection_engine;
};

} // namespace

#endif  /*!PROTON_CPP_CONNECTION_OPTIONS_H*/
