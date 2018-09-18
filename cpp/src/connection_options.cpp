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
#include "proton/connection_options.hpp"

#include "proton/connection.hpp"
#include "proton/codec/vector.hpp"
#include "proton/fwd.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/reconnect_options.hpp"
#include "proton/transport.hpp"
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
#include "proton_bits.hpp"

#include <proton/connection.h>
#include <proton/proactor.h>
#include <proton/transport.h>

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

class connection_options::impl {
  public:
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

    /*
     * There are three types of connection options: the handler
     * (required at creation, so too late to apply here), open frame
     * options (that never change after the original open), and
     * transport options (set once per transport over the life of the
     * connection).
     */
    void apply_unbound(connection& c) {
        pn_connection_t *pnc = unwrap(c);

        // Only apply connection options if uninit.
        bool uninit = c.uninitialized();
        if (!uninit) return;

        if (reconnect.set)
            connection_context::get(pnc).reconnect_context_.reset(new reconnect_context(reconnect.value));
        if (container_id.set)
            pn_connection_set_container(pnc, container_id.value.c_str());
        if (virtual_host.set)
            pn_connection_set_hostname(pnc, virtual_host.value.c_str());
        if (user.set)
            pn_connection_set_user(pnc, user.value.c_str());
        if (password.set)
            pn_connection_set_password(pnc, password.value.c_str());
        if (offered_capabilities.set)
            value(pn_connection_offered_capabilities(pnc)) = offered_capabilities.value;
        if (desired_capabilities.set)
            value(pn_connection_desired_capabilities(pnc)) = desired_capabilities.value;
    }

    void apply_transport(pn_transport_t* pnt) {
        if (max_frame_size.set)
            pn_transport_set_max_frame(pnt, max_frame_size.value);
        if (max_sessions.set)
            pn_transport_set_channel_max(pnt, max_sessions.value);
        if (idle_timeout.set)
            pn_transport_set_idle_timeout(pnt, idle_timeout.value.milliseconds());
    }

    void apply_sasl(pn_transport_t* pnt) {
        // Transport options.  pnt is NULL between reconnect attempts
        // and if there is a pipelined open frame.
        if (!pnt) return;

        // Skip entirely if SASL explicitly disabled
        if (!sasl_enabled.set || sasl_enabled.value) {
            if (sasl_enabled.set)  // Explicitly set, not just default behaviour.
                pn_sasl(pnt);      // Force a sasl instance.  Lazily create one otherwise.
            if (sasl_allow_insecure_mechs.set)
                pn_sasl_set_allow_insecure_mechs(pn_sasl(pnt), sasl_allow_insecure_mechs.value);
            if (sasl_allowed_mechs.set)
                pn_sasl_allowed_mechs(pn_sasl(pnt), sasl_allowed_mechs.value.c_str());
            if (sasl_config_name.set)
                pn_sasl_config_name(pn_sasl(pnt), sasl_config_name.value.c_str());
            if (sasl_config_path.set)
                pn_sasl_config_path(pn_sasl(pnt), sasl_config_path.value.c_str());
        }

    }

    void apply_ssl(pn_transport_t* pnt, bool client) {
        // Transport options.  pnt is NULL between reconnect attempts
        // and if there is a pipelined open frame.
        if (!pnt) return;

        if (client && ssl_client_options.set) {
            // A side effect of pn_ssl() is to set the ssl peer
            // hostname to the connection hostname, which has
            // already been adjusted for the virtual_host option.
            pn_ssl_t *ssl = pn_ssl(pnt);
            if (pn_ssl_init(ssl, ssl_client_options.value.pn_domain(), NULL))
                throw error(MSG("client SSL/TLS initialization error"));
        } else if (!client && ssl_server_options.set) {
                pn_ssl_t *ssl = pn_ssl(pnt);
                if (pn_ssl_init(ssl, ssl_server_options.value.pn_domain(), NULL))
                    throw error(MSG("server SSL/TLS initialization error"));
        }

    }

    void update(const impl& x) {
        handler.update(x.handler);
        max_frame_size.update(x.max_frame_size);
        max_sessions.update(x.max_sessions);
        idle_timeout.update(x.idle_timeout);
        container_id.update(x.container_id);
        virtual_host.update(x.virtual_host);
        user.update(x.user);
        password.update(x.password);
        offered_capabilities.update(x.offered_capabilities);
        desired_capabilities.update(x.desired_capabilities);
        reconnect.update(x.reconnect);
        ssl_client_options.update(x.ssl_client_options);
        ssl_server_options.update(x.ssl_server_options);
        sasl_enabled.update(x.sasl_enabled);
        sasl_allow_insecure_mechs.update(x.sasl_allow_insecure_mechs);
        sasl_allowed_mechs.update(x.sasl_allowed_mechs);
        sasl_config_name.update(x.sasl_config_name);
        sasl_config_path.update(x.sasl_config_path);
    }

};

connection_options::connection_options() : impl_(new impl()) {}

connection_options::connection_options(class messaging_handler& h) : impl_(new impl()) { handler(h); }

connection_options::connection_options(const connection_options& x) : impl_(new impl()) {
    *this = x;
}

connection_options::~connection_options() {}

connection_options& connection_options::operator=(const connection_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

connection_options& connection_options::update(const connection_options& x) {
    impl_->update(*x.impl_);
    return *this;
}

connection_options& connection_options::handler(class messaging_handler &h) { impl_->handler = &h; return *this; }
connection_options& connection_options::max_frame_size(uint32_t n) { impl_->max_frame_size = n; return *this; }
connection_options& connection_options::max_sessions(uint16_t n) { impl_->max_sessions = n; return *this; }
connection_options& connection_options::idle_timeout(duration t) { impl_->idle_timeout = t; return *this; }
connection_options& connection_options::container_id(const std::string &id) { impl_->container_id = id; return *this; }
connection_options& connection_options::virtual_host(const std::string &id) { impl_->virtual_host = id; return *this; }
connection_options& connection_options::user(const std::string &user) { impl_->user = user; return *this; }
connection_options& connection_options::password(const std::string &password) { impl_->password = password; return *this; }
connection_options& connection_options::offered_capabilities(const std::vector<symbol> &caps) { impl_->offered_capabilities = caps; return *this; }
connection_options& connection_options::desired_capabilities(const std::vector<symbol> &caps) { impl_->desired_capabilities = caps; return *this; }
connection_options& connection_options::reconnect(const reconnect_options &r) { impl_->reconnect = r; return *this; }
connection_options& connection_options::ssl_client_options(const class ssl_client_options &c) { impl_->ssl_client_options = c; return *this; }
connection_options& connection_options::ssl_server_options(const class ssl_server_options &c) { impl_->ssl_server_options = c; return *this; }
connection_options& connection_options::sasl_enabled(bool b) { impl_->sasl_enabled = b; return *this; }
connection_options& connection_options::sasl_allow_insecure_mechs(bool b) { impl_->sasl_allow_insecure_mechs = b; return *this; }
connection_options& connection_options::sasl_allowed_mechs(const std::string &s) { impl_->sasl_allowed_mechs = s; return *this; }
connection_options& connection_options::sasl_config_name(const std::string &n) { impl_->sasl_config_name = n; return *this; }
connection_options& connection_options::sasl_config_path(const std::string &p) { impl_->sasl_config_path = p; return *this; }

void connection_options::apply_unbound(connection& c) const { impl_->apply_unbound(c); }
void connection_options::apply_unbound_client(pn_transport_t *t) const { impl_->apply_sasl(t); impl_->apply_ssl(t, true); impl_->apply_transport(t); }
void connection_options::apply_unbound_server(pn_transport_t *t) const { impl_->apply_sasl(t); impl_->apply_ssl(t, false); impl_->apply_transport(t); }

messaging_handler* connection_options::handler() const { return impl_->handler.value; }

} // namespace proton
