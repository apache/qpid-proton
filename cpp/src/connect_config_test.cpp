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
#include "proton/messaging_handler.hpp"
#include "proton/transport.hpp"

#include <sstream>
#include <fstream>
#include <cstdio>

#include <stdlib.h>

namespace {

using namespace std;
using namespace proton;
using proton::error_condition;

string configure(connection_options& opts, const string& config) {
    istringstream is(config);
    return connect_config::parse(is, opts);
}

void test_default_file() {
    // Default file locations in order of preference.
    ::setenv("MESSAGING_CONNECT_FILE", "environment", 1);
    ofstream("connect.json") << "{ \"host\": \"current\" }" << endl;
    ::setenv("HOME", "testdata", 1);
    ofstream("testdata/.config/messaging/connect.json") << "{ \"host\": \".config\" }" << endl;
    ASSERT_EQUAL("environment", connect_config::default_file());
    ::unsetenv("MESSAGING_CONNECT_FILE");
    ASSERT_EQUAL("connect.json", connect_config::default_file());
    remove("connect.json");
    ASSERT_EQUAL("testdata/.config/messaging/connect.json", connect_config::default_file());
    remove("testdata/.config/messaging/connect.json");

    // We can't fully test prefix and /etc locations, we have no control.
    try {
        ASSERT_SUBSTRING("/etc/messaging", connect_config::default_file());
    } catch (...) {}            // OK if not there
}

void test_addr() {
    connection_options opts;
    ASSERT_EQUAL("foo:bar", configure(opts, "{ \"host\":\"foo\", \"port\":\"bar\" }"));
    ASSERT_EQUAL("foo:1234", configure(opts, "{ \"host\":\"foo\", \"port\":\"1234\" }"));
    ASSERT_EQUAL(":amqps", configure(opts, "{}"));
    ASSERT_EQUAL(":amqp", configure(opts, "{\"scheme\":\"amqp\"}"));
    ASSERT_EQUAL("foo:bar", configure(opts, "{ \"host\":\"foo\", /* inline comment */\"port\":\"bar\" // end of line comment\n}"));

    ASSERT_THROWS_MSG(error, "'scheme' must be", configure(opts, "{\"scheme\":\"bad\"}"));
    ASSERT_THROWS_MSG(error, "'scheme' expected string, found boolean", configure(opts, "{\"scheme\":true}"));
    ASSERT_THROWS_MSG(error, "'port' expected string or integer, found boolean", configure(opts, "{\"port\":true}"));
    ASSERT_THROWS_MSG(error, "'host' expected string, found boolean", configure(opts, "{\"host\":true}"));
}

class test_handler : public messaging_handler {
  protected:
    string config_;
    listener listener_;
    bool opened_;
    proton::error_condition error_;

  public:

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        listener_ = c.listen("//:0");
    }

    virtual void check_connection(connection& c) {}

    void on_connection_open(connection& c) PN_CPP_OVERRIDE {
        if (!c.active()) {      // Server side
            opened_ = true;
            check_connection(c);
            listener_.stop();
            c.close();
        }
    }

    void on_error(const error_condition& e) PN_CPP_OVERRIDE {
        FAIL("unexpected error " << e);
    }

    void run(const string& config) {
        config_ = config;
        container(*this).run();
    }
};

class test_default_connect : public test_handler {
  public:

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        ofstream os("connect.json");
        ASSERT(os << "{ \"port\": " << listener_.port() << "}" << endl);
        os.close();
        c.connect();
    }

    void run() {
        container(*this).run();
        ASSERT(opened_);
        ASSERT(!error_);
    }
};

} // namespace


int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_default_file());
    RUN_ARGV_TEST(failed, test_addr());
    RUN_ARGV_TEST(failed, test_default_connect().run());
    return failed;
}
