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
#include "proton/reconnect_timer.hpp"
#include "proton/transport.hpp"
#include "contexts.hpp"
#include "connector.hpp"
#include "msg.hpp"

#include "proton/transport.h"

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void override(const option<T>& x) { if (x.set) *this = x.value; }
};

class connection_options::impl {
  public:
    option<class handler*> handler;
    option<uint32_t> max_frame_size;
    option<uint16_t> max_channels;
    option<uint32_t> idle_timeout;
    option<uint32_t> heartbeat;
    option<std::string> container_id;
    option<reconnect_timer> reconnect;
#ifdef PN_CCP_SOON
    option<class client_domain> client_domain;
    option<class server_domain> server_domain;
    option<std::string> peer_hostname;
    option<std::string> resume_id;
    option<bool> sasl_enabled;
    option<std::string> allowed_mechs;
    option<bool> allow_insecure_mechs;
#endif

    void apply(connection& c) {
        pn_connection_t *pnc = connection_options::pn_connection(c);
        pn_transport_t *pnt = pn_connection_transport(pnc);
        connector *outbound = dynamic_cast<connector*>(c.context().handler.get());
        bool uninit = (c.state() & endpoint::LOCAL_UNINIT);

        // pnt is NULL between reconnect attempts.
        // Only apply transport options if uninit or outbound with
        // transport not yet configured.
        if (pnt && (uninit || (outbound && !outbound->transport_configured())))
        {
            if (max_frame_size.set)
                pn_transport_set_max_frame(pnt, max_frame_size.value);
            if (max_channels.set)
                pn_transport_set_channel_max(pnt, max_channels.value);
            if (idle_timeout.set)
                pn_transport_set_idle_timeout(pnt, idle_timeout.value);
        }
        // Only apply connection options if uninit.
        if (uninit) {
            if (reconnect.set && outbound)
                outbound->reconnect_timer(reconnect.value);
            if (container_id.set)
                pn_connection_set_container(pnc, container_id.value.c_str());
        }
    }

    void override(const impl& x) {
        handler.override(x.handler);
        max_frame_size.override(x.max_frame_size);
        max_channels.override(x.max_channels);
        idle_timeout.override(x.idle_timeout);
        heartbeat.override(x.heartbeat);
        container_id.override(x.container_id);
        reconnect.override(x.reconnect);
    }

};

connection_options::connection_options() : impl_(new impl()) {}
connection_options::connection_options(const connection_options& x) : impl_(new impl()) {
    *this = x;
}
connection_options::~connection_options() {}

connection_options& connection_options::operator=(const connection_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

void connection_options::override(const connection_options& x) { impl_->override(*x.impl_); }

connection_options& connection_options::handler(class handler *h) { impl_->handler = h; return *this; }
connection_options& connection_options::max_frame_size(uint32_t n) { impl_->max_frame_size = n; return *this; }
connection_options& connection_options::max_channels(uint16_t n) { impl_->max_frame_size = n; return *this; }
connection_options& connection_options::idle_timeout(uint32_t t) { impl_->idle_timeout = t; return *this; }
connection_options& connection_options::heartbeat(uint32_t t) { impl_->heartbeat = t; return *this; }
connection_options& connection_options::container_id(const std::string &id) { impl_->container_id = id; return *this; }
connection_options& connection_options::reconnect(const reconnect_timer &rc) { impl_->reconnect = rc; return *this; }

void connection_options::apply(connection& c) const { impl_->apply(c); }
handler* connection_options::handler() const { return impl_->handler.value; }

pn_connection_t* connection_options::pn_connection(connection &c) { return c.pn_object(); }
} // namespace proton
