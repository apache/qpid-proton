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

#include "proton/receiver_options.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/source_options.hpp"
#include "proton/target_options.hpp"

#include <proton/link.h>

#include "contexts.hpp"
#include "proactor_container_impl.hpp"
#include "messaging_adapter.hpp"
#include "proton_bits.hpp"

namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

class receiver_options::impl {
    static link_context& get_context(receiver l) {
        return link_context::get(unwrap(l));
    }

    static void set_delivery_mode(receiver l, proton::delivery_mode mode) {
        switch (mode) {
        case delivery_mode::AT_MOST_ONCE:
            pn_link_set_snd_settle_mode(unwrap(l), PN_SND_SETTLED);
            break;
        case delivery_mode::AT_LEAST_ONCE:
            pn_link_set_snd_settle_mode(unwrap(l), PN_SND_UNSETTLED);
            pn_link_set_rcv_settle_mode(unwrap(l), PN_RCV_FIRST);
            break;
        default:
            break;
        }
    }

  public:
    option<messaging_handler*> handler;
    option<proton::delivery_mode> delivery_mode;
    option<bool> auto_accept;
    option<bool> auto_settle;
    option<int> credit_window;
    option<bool> dynamic_address;
    option<source_options> source;
    option<target_options> target;
    option<std::string> name;


    void apply(receiver& r) {
        if (r.uninitialized()) {
            if (delivery_mode.set) set_delivery_mode(r, delivery_mode.value);
            if (handler.set && handler.value) container::impl::set_handler(r, handler.value);
            if (auto_settle.set) get_context(r).auto_settle = auto_settle.value;
            if (auto_accept.set) get_context(r).auto_accept = auto_accept.value;
            if (credit_window.set) get_context(r).credit_window = credit_window.value;

            if (source.set) {
                proton::source local_s(make_wrapper<proton::source>(pn_link_source(unwrap(r))));
                source.value.apply(local_s);
            }
            if (target.set) {
                proton::target local_t(make_wrapper<proton::target>(pn_link_target(unwrap(r))));
                target.value.apply(local_t);
            }
        }
    }

    void update(const impl& x) {
        handler.update(x.handler);
        delivery_mode.update(x.delivery_mode);
        auto_accept.update(x.auto_accept);
        auto_settle.update(x.auto_settle);
        credit_window.update(x.credit_window);
        dynamic_address.update(x.dynamic_address);
        source.update(x.source);
        target.update(x.target);
        name.update(x.name);
    }

};

receiver_options::receiver_options() : impl_(new impl()) {}
receiver_options::receiver_options(const receiver_options& x) : impl_(new impl()) {
    *this = x;
}
receiver_options::~receiver_options() {}

receiver_options& receiver_options::operator=(const receiver_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

void receiver_options::update(const receiver_options& x) { impl_->update(*x.impl_); }

receiver_options& receiver_options::handler(class messaging_handler &h) { impl_->handler = &h; return *this; }
receiver_options& receiver_options::delivery_mode(proton::delivery_mode m) {impl_->delivery_mode = m; return *this; }
receiver_options& receiver_options::auto_accept(bool b) {impl_->auto_accept = b; return *this; }
receiver_options& receiver_options::credit_window(int w) {impl_->credit_window = w; return *this; }
receiver_options& receiver_options::source(source_options &s) {impl_->source = s; return *this; }
receiver_options& receiver_options::target(target_options &s) {impl_->target = s; return *this; }
receiver_options& receiver_options::name(const std::string &s) {impl_->name = s; return *this; }

void receiver_options::apply(receiver& r) const { impl_->apply(r); }

const std::string* receiver_options::get_name() const {
    return impl_->name.set ? &impl_->name.value : 0;
}

// No-op, kept for binary compat but auto_settle is not relevant to receiver only sender.
receiver_options& receiver_options::auto_settle(bool b) { return *this; }

} // namespace proton
