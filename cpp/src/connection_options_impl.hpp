#ifndef PROTON_CONNECTION_OPTIONS_IMPL_HPP
#define PROTON_CONNECTION_OPTIONS_IMPL_HPP

/*
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
 */

#include "proton/codec/vector.hpp"
#include "proton/connection.h"
#include "proton/connection_options.hpp"
#include "proton/reconnect_options.hpp"
#include "proton/sasl.hpp"
#include "proton/ssl.hpp"
#include "proton/transport.h"
#include "proton_bits.hpp"

namespace proton {

class connection_options_impl {
  public:
    template <class T> struct option {
        T value;
        bool set;

        option() : value(), set(false) {}
        option& operator=(const T& x) { value = x;  set = true; return *this; }
        void update(const option<T>& x) { if (x.set) *this = x.value; }
    };

    static connection_options_impl& get(const connection_options& opts) { return *opts.impl_; }

    /*
     * There are three types of connection options: the handler
     * (required at creation, so too late to apply here), open frame
     * options (that never change after the original open), and
     * transport options (set once per transport over the life of the
     * connection).
     */
    void apply_unbound(connection& c);
    void apply_unbound_client(pn_transport_t *t);
    void apply_unbound_server(pn_transport_t *t);
    void apply_transport(pn_transport_t* pnt);
    void apply_sasl(pn_transport_t* pnt);
    void apply_ssl(pn_transport_t* pnt, bool client);

    void update(const connection_options_impl& x);

    option<messaging_handler*> handler;
    option<uint32_t> max_frame_size;
    option<uint16_t> max_sessions;
    option<duration> idle_timeout;
    option<std::string> container_id;
    option<std::string> virtual_host;
    option<std::string> user;
    option<std::string> password;
    option<std::vector<symbol> > offered_capabilities;
    option<std::vector<symbol> > desired_capabilities;
    option<reconnect_options> reconnect;
    option<class ssl_client_options> ssl_client_options;
    option<class ssl_server_options> ssl_server_options;
    option<bool> sasl_enabled;
    option<std::string> sasl_allowed_mechs;
    option<bool> sasl_allow_insecure_mechs;
    option<std::string> sasl_config_name;
    option<std::string> sasl_config_path;
};

} // namespace proton

#endif // PROTON_CONNECTION_OPTIONS_IMPL_HPP
