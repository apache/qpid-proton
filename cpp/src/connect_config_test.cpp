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
#include "proton/transport.hpp"
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"

// The C++ API lacks a way to test for presence of extended SASL or SSL support.
#include "proton/sasl.h"
#include "proton/ssl.h"

#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

// Used to inspect items from the proton-c level of the API
#include "proton/internal/object.hpp"
#include "proton/returned.hpp"
#include <proton/transport.h>
#include <proton/connection.h>

#include <json/version.h>

// Windows has a different set of APIs for setting the environment
#ifdef _WIN32
#include <string.h>
namespace {
    void setenv(const char* var, const char* value, int) {
        int vlen = strlen(var);
        int len = vlen+strlen(value)+1;
        char* buff = new char[len+1];
        strcpy(buff, var);
        strcpy(buff+vlen, "=");
        strcpy(buff+vlen+1, value);
        _putenv(buff);
        delete [] buff;
    }

    void unsetenv(const char* var) {
        int len = strlen(var);
        char* buff = new char[len+1];
        strcpy(buff, var);
        strcpy(buff+len, "=");
        _putenv(buff);
        delete [] buff;
    }
}

#endif
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

void test_invalid_config() {
    connection_options opts;
    ASSERT_THROWS_MSG(proton::error, "expected string", configure(opts, RAW_STRING({ "scheme":true})));
    ASSERT_THROWS_MSG(proton::error, "expected object", configure(opts, RAW_STRING({ "tls":""})));
    ASSERT_THROWS_MSG(proton::error, "expected object", configure(opts, RAW_STRING({ "sasl":true})));
    ASSERT_THROWS_MSG(proton::error, "expected boolean", configure(opts, RAW_STRING({ "sasl": { "enable":""}})));
    ASSERT_THROWS_MSG(proton::error, "expected boolean", configure(opts, RAW_STRING({ "tls": { "verify":""}})));
}

void test_invalid_json() {
  connection_options opts;
  // ancient versions of jsoncpp use a generic error message for parse errors
  //  in the exception, and print the detailed message to stderr
  // https://github.com/open-source-parsers/jsoncpp/commit/6b10ce8c0d07ea07861e82f65f17d9fd6abd658d
  if (std::make_tuple(JSONCPP_VERSION_MAJOR, JSONCPP_VERSION_MINOR) < std::make_tuple(1, 7)) {
    ASSERT_THROWS_MSG(proton::error, "reader error", configure(opts, "{"));
    ASSERT_THROWS_MSG(proton::error, "reader error", configure(opts, ""));
    ASSERT_THROWS_MSG(proton::error, "reader error", configure(opts, RAW_STRING({ "user" : "x" "host" : "y"})));
  } else {
    ASSERT_THROWS_MSG(proton::error, "Missing '}'", configure(opts, "{"));
    ASSERT_THROWS_MSG(proton::error, "Syntax error", configure(opts, ""));
    ASSERT_THROWS_MSG(proton::error, "Missing ','", configure(opts, RAW_STRING({ "user":"x" "host":"y"})));
  }
}

// Extra classes to resolve clash of on_error in both messaging_handler and listen_handler
class messaging_handler : public proton::messaging_handler {
    virtual void on_messaging_error(const error_condition&) = 0;

    void on_error(const error_condition& c) PN_CPP_OVERRIDE {
        on_messaging_error(c);
    }
};

class listen_handler : public proton::listen_handler {
    virtual void on_listen_error(listener& , const string&) = 0;

    void on_error(listener& l, const string& s) PN_CPP_OVERRIDE {
        on_listen_error(l, s);
    }
};

class test_handler : public messaging_handler,  public listen_handler {
    bool opened_;
    connection_options connection_options_;
    listener listener_;

    void on_open(listener& l) PN_CPP_OVERRIDE {
        on_listener_start(l.container());
    }

    connection_options on_accept(listener& l) PN_CPP_OVERRIDE {
        return connection_options_;
    }

    void on_container_start(container& c) PN_CPP_OVERRIDE {
        listener_ = c.listen("//:0", *this);
    }

    void on_connection_open(connection& c) PN_CPP_OVERRIDE {
        if (!c.active()) {      // Server side
            opened_ = true;
            check_connection(c);
            listener_.stop();
            c.close();
        }
    }

    void on_messaging_error(const error_condition& e) PN_CPP_OVERRIDE {
        FAIL("unexpected error " << e);
    }

    void on_listen_error(listener&, const string& s) PN_CPP_OVERRIDE {
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

class test_almost_default_connect : public test_handler {
  public:

    void on_listener_start(container& c) PN_CPP_OVERRIDE {
        ofstream os("connect.json");
        ASSERT(os << config_with_port(RAW_STRING("scheme":"amqp")));
        os.close();
        c.connect();
    }

    void check_connection(connection& c) PN_CPP_OVERRIDE {
        ASSERT_EQUAL("localhost", c.virtual_host());
    }
};

// Hack to use protected pn_object member of proton::object
template <class P, class T>
class internal_access_of: public P {

public:
    internal_access_of(const P& t) : P(t) {}
    T* pn_object() { return proton::internal::object<T>::pn_object(); }
};

class test_default_connect : public test_handler {
  public:

    void on_listener_start(container& c) PN_CPP_OVERRIDE {
        c.connect();
    }

    // on_transport_open will be called whatever happens to the connection whether it's
    // connected or rejected due to error.
    //
    // We use a hacky way to get to the underlying C objects to check what's in them
    void on_transport_open(proton::transport& transport) PN_CPP_OVERRIDE {
        proton::connection connection = transport.connection();
        internal_access_of<proton::connection, pn_connection_t> ic(connection);
        pn_connection_t* c = ic.pn_object();
        ASSERT_EQUAL(std::string("localhost"), pn_connection_get_hostname(c));
    }

    void on_transport_error(proton::transport& t) PN_CPP_OVERRIDE {
        ASSERT_SUBSTRING("localhost:5671", t.error().description());
        t.connection().container().stop();
    }

    void run() {
        container(*this).run();
    }
};

class test_host_user_pass : public test_handler {
  public:

    void on_listener_start(proton::container & c) PN_CPP_OVERRIDE {
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

    void on_listener_start(proton::container & c) PN_CPP_OVERRIDE {
        connect(c, RAW_STRING("scheme":"amqps", "tls": { "verify":false }));
    }
};

class test_tls_default_fail : public test_handler {
    static connection_options make_opts() {
        ssl_certificate cert("testdata/certs/server-certificate.pem",
                             "testdata/certs/server-private-key.pem",
                             "server-password");
        connection_options opts;
        opts.ssl_server_options(ssl_server_options(cert));
        return opts;
    }
    bool failed_;

  public:

    test_tls_default_fail() : test_handler(make_opts()), failed_(false) {}

    void on_listener_start(proton::container& c) PN_CPP_OVERRIDE {
        connect(c, RAW_STRING("scheme":"amqps"));
    }

    void on_messaging_error(const proton::error_condition& c) PN_CPP_OVERRIDE {
        if (failed_) return;

        ASSERT_SUBSTRING("verify failed", c.description());
        failed_ = true;
        stop_listener();
    }

    void run() {
        container(*this).run();
        ASSERT(failed_);
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

    void on_listener_start(container& c) PN_CPP_OVERRIDE {
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

    void on_listener_start(container& c) PN_CPP_OVERRIDE {
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
    RUN_ARGV_TEST(failed, test_invalid_config());
    RUN_ARGV_TEST(failed, test_invalid_json());
    RUN_ARGV_TEST(failed, test_default_connect().run());
    RUN_ARGV_TEST(failed, test_almost_default_connect().run());

    bool have_sasl = pn_sasl_extended() && getenv("PN_SASL_CONFIG_PATH");
    pn_ssl_domain_t *have_ssl = pn_ssl_domain(PN_SSL_MODE_SERVER);

    if (have_sasl) {
        RUN_ARGV_TEST(failed, test_host_user_pass().run());
    }
#ifndef _WIN32
    if (have_ssl) {
        pn_ssl_domain_free(have_ssl);
        RUN_ARGV_TEST(failed, test_tls().run());
        RUN_ARGV_TEST(failed, test_tls_default_fail().run());
        RUN_ARGV_TEST(failed, test_tls_external().run());
        if (have_sasl) {
            RUN_ARGV_TEST(failed, test_tls_plain().run());
        }
    } else {
        std::cout << "SKIP: TLS tests, not available" << std::endl;
    }
#else
    std::cout << "SKIP: TLS tests, expected to fail on windows" << std::endl;
#endif
    return failed;
}
