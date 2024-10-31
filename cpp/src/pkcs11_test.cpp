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

#include "proton/connection_options.hpp"
#include "proton/container.hpp"
#include "proton/ssl.hpp"

// The C++ API lacks a way to test for presence of extended SSL support.
#include "proton/ssl.h"

#include <cstdio>
#include <string>

#include "test_handler.hpp"

#define SKIP_RETURN_CODE  127

namespace {

using namespace std;
using namespace proton;

// Hack to write strings with embedded '"' and newlines
#define RAW_STRING(...) #__VA_ARGS__

static const char *client_cert, *client_key,
             *server_cert, *server_key,
             *ca_cert;

class test_tls_external : public test_handler {

    static connection_options make_opts() {
        ssl_certificate cert(server_cert, server_key);
        connection_options opts;
        opts.ssl_server_options(ssl_server_options(cert, ca_cert,
                             ca_cert, ssl::VERIFY_PEER));
        return opts;
    }

  public:

    test_tls_external() : test_handler(make_opts()) {}

    void on_listener_start(container& c) override {
        static char buf[1024];

        snprintf(buf, sizeof(buf), RAW_STRING(
                    "scheme":"amqps",
                    "sasl":{ "mechanisms": "EXTERNAL" },
                    "tls": {
                            "cert":"%s",
                            "key":"%s",
                            "ca":"%s",
                            "verify":true }),
            client_cert, client_key, ca_cert);

        connect(c, buf);
    }
};

} // namespace

int main(int argc, char** argv) {
    client_cert = getenv("PKCS11_CLIENT_CERT");
    client_key = getenv("PKCS11_CLIENT_KEY");

    server_cert = getenv("PKCS11_SERVER_CERT");
    server_key = getenv("PKCS11_SERVER_KEY");

    ca_cert = getenv("PKCS11_CA_CERT");

    if (!client_key || !client_cert || !server_key || !server_cert || !ca_cert) {
            std::cout << argv[0] << ": Environment variable configuration missing:" << std::endl;
            std::cout << "\tPKCS11_CLIENT_CERT: URI of client certificate" << std::endl;
            std::cout << "\tPKCS11_CLIENT_KEY:  URI of client private key" << std::endl;
            std::cout << "\tPKCS11_SERVER_CERT: URI of server certificate" << std::endl;
            std::cout << "\tPKCS11_SERVER_KEY:  URI of server private key" << std::endl;
            std::cout << "\tPKCS11_CA_CERT:     URI of CA certificate" << std::endl;
            return SKIP_RETURN_CODE;
    }

    int failed = 0;

    pn_ssl_domain_t *have_ssl = pn_ssl_domain(PN_SSL_MODE_SERVER);

    if (!have_ssl) {
        std::cout << "SKIP: TLS tests, not available" << std::endl;
        return SKIP_RETURN_CODE;
    }

    pn_ssl_domain_free(have_ssl);
    RUN_TEST(failed, test_tls_external().run());

    return failed;
}
