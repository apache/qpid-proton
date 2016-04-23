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
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/handler.hpp"

#include "msg.hpp"
#include "messaging_adapter.hpp"
#include "contexts.hpp"


namespace proton {

template <class T> struct option {
    T value;
    bool set;

    option() : value(), set(false) {}
    option& operator=(const T& x) { value = x;  set = true; return *this; }
    void update(const option<T>& x) { if (x.set) *this = x.value; }
};

class sender_options::impl {
  public:
    option<proton_handler*> handler;
    option<enum delivery_mode> delivery_mode;
    option<bool> auto_settle;

    void apply(sender& s) {
        if (s.uninitialized()) {
            if (delivery_mode.set) {
                switch (delivery_mode.value) {
                case AT_MOST_ONCE:
                    s.sender_settle_mode(sender_options::SETTLED);
                    break;
                case AT_LEAST_ONCE:
                        s.sender_settle_mode(sender_options::UNSETTLED);
                        s.receiver_settle_mode(receiver_options::SETTLE_ALWAYS);
                    break;
                default:
                    break;
                }
            }
            if (handler.set) {
                if (handler.value)
                    s.handler(*handler.value);
                else
                    s.detach_handler();
            }
            if (auto_settle.set) s.context().auto_settle = auto_settle.value;
        }
    }

    void update(const impl& x) {
        handler.update(x.handler);
        delivery_mode.update(x.delivery_mode);
        auto_settle.update(x.auto_settle);
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

sender_options& sender_options::handler(class handler *h) { impl_->handler = h->messaging_adapter_.get(); return *this; }
sender_options& sender_options::delivery_mode(enum delivery_mode m) {impl_->delivery_mode = m; return *this; }
sender_options& sender_options::auto_settle(bool b) {impl_->auto_settle = b; return *this; }

void sender_options::apply(sender& s) const { impl_->apply(s); }
proton_handler* sender_options::handler() const { return impl_->handler.value; }

} // namespace proton
