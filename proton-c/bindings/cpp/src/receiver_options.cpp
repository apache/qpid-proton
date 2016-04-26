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

#include "proton/binary.hpp"
#include "proton/receiver.hpp"
#include "proton/receiver_options.hpp"
#include "proton/handler.hpp"
#include "proton/settings.hpp"
#include "proton/source_options.hpp"
#include "proton/target_options.hpp"

#include "proton/link.h"

#include "contexts.hpp"
#include "messaging_adapter.hpp"
#include "msg.hpp"
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
  public:
    option<proton_handler*> handler;
    option<enum delivery_mode> delivery_mode;
    option<bool> auto_accept;
    option<bool> auto_settle;
    option<int> credit_window;
    option<bool> dynamic_address;
    option<source_options> source;
    option<target_options> target;


    void apply(receiver& r) {
        if (r.uninitialized()) {
            if (delivery_mode.set) {
                switch (delivery_mode.value) {
                case AT_MOST_ONCE:
                    r.sender_settle_mode(sender_options::SETTLED);
                    break;
                case AT_LEAST_ONCE:
                    r.sender_settle_mode(sender_options::UNSETTLED);
                    r.receiver_settle_mode(receiver_options::SETTLE_ALWAYS);
                    break;
                default:
                    break;
                }
            }
            if (handler.set) {
                if (handler.value)
                    r.handler(*handler.value);
                else
                    r.detach_handler();
            }

            if (auto_settle.set) r.context().auto_settle = auto_settle.value;
            if (auto_accept.set) r.context().auto_accept = auto_accept.value;
            if (credit_window.set) r.context().credit_window = credit_window.value;

            terminus local_src(make_wrapper(pn_link_source(r.pn_object())));
            if (source.set) {
                proton::source local_s(pn_link_source(r.pn_object()));
                source.value.apply(local_s);
            }
            if (target.set) {
                proton::target local_t(pn_link_target(r.pn_object()));
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

receiver_options& receiver_options::handler(class handler *h) { impl_->handler = h->messaging_adapter_.get(); return *this; }
receiver_options& receiver_options::delivery_mode(enum delivery_mode m) {impl_->delivery_mode = m; return *this; }
receiver_options& receiver_options::auto_accept(bool b) {impl_->auto_accept = b; return *this; }
receiver_options& receiver_options::auto_settle(bool b) {impl_->auto_settle = b; return *this; }
receiver_options& receiver_options::credit_window(int w) {impl_->credit_window = w; return *this; }
receiver_options& receiver_options::source(source_options &s) {impl_->source = s; return *this; }
receiver_options& receiver_options::target(target_options &s) {impl_->target = s; return *this; }
receiver_options& receiver_options::selector(const std::string&) { return *this; }

void receiver_options::apply(receiver& r) const { impl_->apply(r); }
proton_handler* receiver_options::handler() const { return impl_->handler.value; }

} // namespace proton
