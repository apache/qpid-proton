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
#include "proton/link_options.hpp"
#include "proton/link.hpp"
#include "msg.hpp"


namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void override(const option<T>& x) { if (x.set) *this = x.value; }
};

class link_options::impl {
  public:
    option<class handler*> handler;
    option<terminus::distribution_mode_t> distribution_mode;
    option<bool> durable_subscription;
    option<link_delivery_mode_t> delivery_mode;
    option<bool> dynamic_address;
    option<std::string> local_address;

    void apply(link& l) {
        if (l.state() & endpoint::LOCAL_UNINIT) {
            bool sender = !l.receiver();
            if (local_address.set) {
                const char *addr = local_address.value.empty() ? NULL : local_address.value.c_str();
                if (sender)
                    l.target().address(addr);
                else
                    l.source().address(addr);
            }
            if (delivery_mode.set) {
                switch (delivery_mode.value) {
                case AT_MOST_ONCE:
                    l.sender_settle_mode(link::SETTLED);
                    break;
                case AT_LEAST_ONCE:
                        l.sender_settle_mode(link::UNSETTLED);
                        l.receiver_settle_mode(link::SETTLE_ALWAYS);
                    break;
                default:
                    break;
                }
            }
            if (handler.set) {
                if (handler.value)
                    l.handler(*handler.value);
                else
                    l.detach_handler();
            }
            if (!sender) {
                // receiver only options
                if (distribution_mode.set) l.source().distribution_mode(distribution_mode.value);
                if (durable_subscription.set && durable_subscription.value) {
                    l.source().durability(terminus::DELIVERIES);
                    l.source().expiry_policy(terminus::EXPIRE_NEVER);
                }
                if (dynamic_address.set)
                    l.source().dynamic(dynamic_address.value);
            }
        }
    }

    void override(const impl& x) {
        handler.override(x.handler);
        distribution_mode.override(x.distribution_mode);
        durable_subscription.override(x.durable_subscription);
        delivery_mode.override(x.delivery_mode);
        dynamic_address.override(x.dynamic_address);
        local_address.override(x.local_address);
    }

};

link_options::link_options() : impl_(new impl()) {}
link_options::link_options(const link_options& x) : impl_(new impl()) {
    *this = x;
}
link_options::~link_options() {}

link_options& link_options::operator=(const link_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

void link_options::override(const link_options& x) { impl_->override(*x.impl_); }

link_options& link_options::handler(class handler *h) { impl_->handler = h; return *this; }
link_options& link_options::browsing(bool b) { distribution_mode(b ? terminus::COPY : terminus::MOVE); return *this; }
link_options& link_options::distribution_mode(terminus::distribution_mode_t m) { impl_->distribution_mode = m; return *this; }
link_options& link_options::durable_subscription(bool b) {impl_->durable_subscription = b; return *this; }
link_options& link_options::delivery_mode(link_delivery_mode_t m) {impl_->delivery_mode = m; return *this; }
link_options& link_options::dynamic_address(bool b) {impl_->dynamic_address = b; return *this; }
link_options& link_options::local_address(const std::string &addr) {impl_->local_address = addr; return *this; }

void link_options::apply(link& l) const { impl_->apply(l); }
handler* link_options::handler() const { return impl_->handler.value; }

} // namespace proton
