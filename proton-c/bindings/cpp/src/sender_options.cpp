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

#include "proton/sender_options.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/source_options.hpp"
#include "proton/target_options.hpp"

#include "proactor_container_impl.hpp"
#include "contexts.hpp"
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

class sender_options::impl {
    static link_context& get_context(sender l) {
        return link_context::get(unwrap(l));
    }

    static void set_delivery_mode(sender l, proton::delivery_mode mode) {
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
    option<bool> auto_settle;
    option<source_options> source;
    option<target_options> target;
    option<std::string> name;

    void apply(sender& s) {
        if (s.uninitialized()) {
            if (delivery_mode.set) set_delivery_mode(s, delivery_mode.value);
            if (handler.set && handler.value) container::impl::set_handler(s, handler.value);
            if (auto_settle.set) get_context(s).auto_settle = auto_settle.value;
            if (source.set) {
                proton::source local_s(make_wrapper<proton::source>(pn_link_source(unwrap(s))));
                source.value.apply(local_s);
            }
            if (target.set) {
                proton::target local_t(make_wrapper<proton::target>(pn_link_target(unwrap(s))));
                target.value.apply(local_t);
            }
        }
    }

    void update(const impl& x) {
        handler.update(x.handler);
        delivery_mode.update(x.delivery_mode);
        auto_settle.update(x.auto_settle);
        source.update(x.source);
        target.update(x.target);
        name.update(x.name);
    }

};

sender_options::sender_options() : impl_(new impl()) {}
sender_options::sender_options(const sender_options& x) : impl_(new impl()) {
    *this = x;
}
sender_options::~sender_options() {}

sender_options& sender_options::operator=(const sender_options& x) {
    *impl_ = *x.impl_;
    return *this;
}

void sender_options::update(const sender_options& x) { impl_->update(*x.impl_); }

sender_options& sender_options::handler(class messaging_handler &h) { impl_->handler = &h; return *this; }
sender_options& sender_options::delivery_mode(proton::delivery_mode m) {impl_->delivery_mode = m; return *this; }
sender_options& sender_options::auto_settle(bool b) {impl_->auto_settle = b; return *this; }
sender_options& sender_options::source(const source_options &s) {impl_->source = s; return *this; }
sender_options& sender_options::target(const target_options &s) {impl_->target = s; return *this; }
sender_options& sender_options::name(const std::string &s) {impl_->name = s; return *this; }

void sender_options::apply(sender& s) const { impl_->apply(s); }

const std::string* sender_options::get_name() const {
    return impl_->name.set ? &impl_->name.value : 0;
}
} // namespace proton
