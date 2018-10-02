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
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"

// The C++ API lacks a way to test for presence of extended SASL or SSL support.
#include "proton/sasl.h"
#include "proton/ssl.h"

#include <sstream>
#include <fstream>
#include <cstdio>

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
    ASSERT_EQUAL("localhost:amqps", configure(opts, "{}"));
    ASSERT_EQUAL("localhost:amqp", configure(opts, "{\"scheme\":\"amqp\"}"));
    ASSERT_EQUAL("foo:bar", configure(opts, "{ \"host\":\"foo\", /* inline comment */\"port\":\"bar\" // end of line comment\n}"));

    ASSERT_THROWS_MSG(error, "'scheme' must be", configure(opts, "{\"scheme\":\"bad\"}"));
    ASSERT_THROWS_MSG(error, "'scheme' expected string, found boolean", configure(opts, "{\"scheme\":true}"));
    ASSERT_THROWS_MSG(error, "'port' expected string or uint, found boolean", configure(opts, "{\"port\":true}"));
    ASSERT_THROWS_MSG(error, "'host' expected string, found boolean", configure(opts, "{\"host\":true}"));
}

// Hack to write strings with embedded '"' and newlines
#define RAW_STRING(...) #__VA_ARGS__

void test_invalid() {
    connection_options opts;
    ASSERT_THROWS_MSG(proton::error, "Missing '}'", configure(opts, "{"));
    ASSERT_THROWS_MSG(proton::error, "Syntax error", configure(opts, ""));
    ASSERT_THROWS_MSG(proton::error, "Missing ','", configure(opts, RAW_STRING({ "user":"x" "host":"y"})));
    ASSERT_THROWS_MSG(proton::error, "expected string", configure(opts, RAW_STRING({ "scheme":true})));
    ASSERT_THROWS_MSG(proton::error, "expected object", configure(opts, RAW_STRING({ "tls":""})));
    ASSERT_THROWS_MSG(proton::error, "expected object", configure(opts, RAW_STRING({ "sasl":true})));
    ASSERT_THROWS_MSG(proton::error, "expected boolean", configure(opts, RAW_STRING({ "sasl": { "enable":""}})));
    ASSERT_THROWS_MSG(proton::error, "expected boolean", configure(opts, RAW_STRING({ "tls": { "verify":""}})));
}

class test_handler : public messaging_handler {
  protected:
    bool opened_;
    connection_options listen_opts_;
    listener listener_;

  public:
    test_handler(const connection_options& listen_opts = connection_options()) :
        opened_(false), listen_opts_(listen_opts) {}

    string config_with_port(const string& bare_config) {
        ostringstream ss;
        ss << "{" << "\"port\":" << listener_.port() << ", " << bare_config << "}";
        return ss.str();
    }

    void connect(container& c, const string& bare_config) {
        connection_options opts;
        c.connect(configure(opts, config_with_port(bare_config)), opts);
    }

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        listener_ = c.listen("//:0", listen_opts_);
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

    void run() {
        container(*this).run();
        ASSERT(opened_);
    }
};

class test_default_connect : public test_handler {
  public:

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        ofstream os("connect.json");
        ASSERT(os << config_with_port(RAW_STRING("scheme":"amqp")));
        os.close();
        c.connect();
    }

    void check_connection(connection& c) PN_CPP_OVERRIDE {
        ASSERT_EQUAL("localhost", c.virtual_host());
    }
};

class test_host_user_pass : public test_handler {
  public:

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        connect(c, RAW_STRING("scheme":"amqp", "host":"127.0.0.1", "user":"user@proton", "password":"password"));
    }

    void check_connection(connection& c) PN_CPP_OVERRIDE {
        ASSERT_EQUAL("127.0.0.1", c.virtual_host());
        if (pn_sasl_extended()) {
            ASSERT_EQUAL("user@proton", c.user());
        } else {
            ASSERT_EQUAL("anonymous", c.user());
        }
    }
};

class test_tls : public test_handler {
    static connection_options make_opts() {
        ssl_certificate cert("testdata/certs/server-certificate.pem",
                             "testdata/certs/server-private-key.pem",
                             "server-password");
        connection_options opts;
        opts.ssl_server_options(ssl_server_options(cert));
        return opts;
    }

  public:

    test_tls() : test_handler(make_opts()) {}

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        connect(c, RAW_STRING("scheme":"amqps", "tls": { "verify":false }));
    }
};

class test_tls_external : public test_handler {

    static connection_options make_opts() {
        ssl_certificate cert("testdata/certs/server-certificate-lh.pem",
                             "testdata/certs/server-private-key-lh.pem",
                             "server-password");
        connection_options opts;
        opts.ssl_server_options(ssl_server_options(cert,
                                                   "testdata/certs/ca-certificate.pem",
                                                   "testdata/certs/ca-certificate.pem",
                                                   ssl::VERIFY_PEER));
        return opts;
    }

  public:

    test_tls_external() : test_handler(make_opts()) {}

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        connect(c, RAW_STRING(
                    "scheme":"amqps",
                    "sasl":{ "mechanisms": "EXTERNAL" },
                    "tls": {
                            "cert":"testdata/certs/client-certificate.pem",
                            "key":"testdata/certs/client-private-key-no-password.pem",
                            "ca":"testdata/certs/ca-certificate.pem",
                            "verify":true }));
    }
};

class test_tls_plain : public test_handler {

    static connection_options make_opts() {
        ssl_certificate cert("testdata/certs/server-certificate-lh.pem",
                             "testdata/certs/server-private-key-lh.pem",
                             "server-password");
        connection_options opts;
        opts.ssl_server_options(ssl_server_options(cert,
                                                   "testdata/certs/ca-certificate.pem",
                                                   "testdata/certs/ca-certificate.pem",
                                                   ssl::VERIFY_PEER));
        return opts;
    }

  public:

    test_tls_plain() : test_handler(make_opts()) {}

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        test_handler::on_container_start(c);
        connect(c, RAW_STRING(
                    "scheme":"amqps", "user":"user@proton", "password": "password",
                    "sasl":{ "mechanisms": "PLAIN" },
                    "tls": {
                            "cert":"testdata/certs/client-certificate.pem",
                            "key":"testdata/certs/client-private-key-no-password.pem",
                            "ca":"testdata/certs/ca-certificate.pem",
                            "verify":true }));
    }
};

} // namespace


int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_default_file());
    RUN_ARGV_TEST(failed, test_addr());
    RUN_ARGV_TEST(failed, test_invalid());
    RUN_ARGV_TEST(failed, test_default_connect().run());

    bool have_sasl = pn_sasl_extended() && getenv("PN_SASL_CONFIG_PATH");
    pn_ssl_domain_t *have_ssl = pn_ssl_domain(PN_SSL_MODE_SERVER);

    if (have_sasl) {
        RUN_ARGV_TEST(failed, test_host_user_pass().run());
    }
    if (have_ssl) {
        pn_ssl_domain_free(have_ssl);
        RUN_ARGV_TEST(failed, test_tls().run());
        RUN_ARGV_TEST(failed, test_tls_external().run());
        if (have_sasl) {
            RUN_ARGV_TEST(failed, test_tls_plain().run());
        }
    } else {
        std::cout << "SKIP: TLS tests, not available" << std::endl;
    }
    return failed;
}
