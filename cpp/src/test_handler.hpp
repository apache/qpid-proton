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

#include "test_bits.hpp"

#include "proton/connect_config.hpp"
#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/container.hpp"
#include "proton/error_condition.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/messaging_handler.hpp"

#include <string>

namespace {

using namespace std;
using namespace proton;
using proton::error_condition;

static inline string configure(connection_options& opts, const string& config) {
    istringstream is(config);
    return connect_config::parse(is, opts);
}

// Extra classes to resolve clash of on_error in both messaging_handler and listen_handler
class messaging_handler : public proton::messaging_handler {
    virtual void on_messaging_error(const error_condition&) = 0;

    void on_error(const error_condition& c) override {
        on_messaging_error(c);
    }
};

class listen_handler : public proton::listen_handler {
    virtual void on_listen_error(listener& , const string&) = 0;

    void on_error(listener& l, const string& s) override {
        on_listen_error(l, s);
    }
};

class test_handler : public messaging_handler,  public listen_handler {
    bool opened_;
    connection_options connection_options_;
    listener listener_;

    void on_open(listener& l) override {
        on_listener_start(l.container());
    }

    connection_options on_accept(listener& l) override {
        return connection_options_;
    }

    void on_container_start(container& c) override {
        listener_ = c.listen("//:0", *this);
    }

    void on_connection_open(connection& c) override {
        if (!c.active()) {      // Server side
            opened_ = true;
            check_connection(c);
            listener_.stop();
            c.close();
        }
    }

    void on_messaging_error(const error_condition& e) override {
        FAIL("unexpected error " << e);
    }

    void on_listen_error(listener&, const string& s) override {
        FAIL("unexpected listen error " << s);
    }

    virtual void check_connection(connection& c) {}
    virtual void on_listener_start(container& c) = 0;

  protected:
    string config_with_port(const string& bare_config) {
        ostringstream ss;
        ss << "{" << "\"port\":" << listener_.port() << ", " << bare_config << "}";
        return ss.str();
    }

    void connect(container& c, const string& bare_config) {
        connection_options opts;
        c.connect(configure(opts, config_with_port(bare_config)), opts);
    }

    void stop_listener() {
        listener_.stop();
    }

  public:
    test_handler(const connection_options& listen_opts = connection_options()) :
        opened_(false), connection_options_(listen_opts) {}

    void run() {
        container(*this).run();
        ASSERT(opened_);
    }
};

// Hack to use protected pn_object member of proton::object
template <class P, class T>
class internal_access_of: public P {

public:
    internal_access_of(const P& t) : P(t) {}
    T* pn_object() { return proton::internal::object<T>::pn_object(); }
};

}
