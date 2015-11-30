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

#include "proton/acceptor.hpp"
#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/connection_options.hpp"
#include "proton/transport.hpp"
#include "proton/ssl.hpp"

#include <iostream>

using proton::connection_options;
using proton::client_domain;
using proton::server_domain;
using proton::ssl_certificate;

// Helper functions defined below.
bool using_OpenSSL();
std::string platform_CA(const std::string &base_name);
ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd);
std::string cert_directory;


struct server_handler : public proton::messaging_handler {
    proton::acceptor acceptor;

    void on_connection_opened(proton::event &e) {
        std::cout << "Inbound server connection connected via SSL.  Protocol: " <<
            e.connection().transport().ssl().protocol() << std::endl;
        acceptor.close();
    }

    void on_message(proton::event &e) {
        std::cout << e.message().body() << std::endl;
    }
};


class hello_world_direct : public proton::messaging_handler {
  private:
    proton::url url;
    server_handler s_handler;

  public:
    hello_world_direct(const proton::url& u) : url(u) {}

    void on_start(proton::event &e) {
        // Configure listener.  Details vary by platform.
        ssl_certificate server_cert = platform_certificate("tserver", "tserverpw");
        server_domain sdomain(server_cert);
        connection_options server_opts;
        server_opts.server_domain(sdomain).handler(&s_handler);
        e.container().server_connection_options(server_opts);

        // Configure client with a Certificate Authority database populated with the server's self signed certificate.
        connection_options client_opts;
        client_opts.client_domain(platform_CA("tserver"));
        // Validate the server certificate against the known name in the certificate.
        client_opts.peer_hostname("test_server");
#ifdef PN_COMING_SOON
        // Turn off unnecessary SASL processing.
        client_opts.sasl_enabled(false);
#endif
        e.container().client_connection_options(client_opts);

        s_handler.acceptor = e.container().listen(url);
        e.container().open_sender(url);
    }

    void on_connection_opened(proton::event &e) {
        std::cout << "Outgoing client connection connected via SSL.  Server certificate has subject " <<
            e.connection().transport().ssl().remote_subject() << std::endl;
    }

    void on_sendable(proton::event &e) {
        proton::message m;
        m.body("Hello World!");
        e.sender().send(m);
        e.sender().close();
    }

    void on_accepted(proton::event &e) {
        // All done.
        e.connection().close();
    }
};

int main(int argc, char **argv) {
    try {
        // Pick an "unusual" port since we are going to be talking to ourselves, not a broker.
        // Note the use of "amqps" as the URL scheme to denote a TLS/SSL connection.
        std::string url = argc > 1 ? argv[1] : "amqps://127.0.0.1:8888/examples";
        // Location of certificates and private key information:
        if (argc > 2) {
            cert_directory = argv[2];
            size_t sz = cert_directory.size();
            if (sz && cert_directory[sz -1] != '/')
                cert_directory.append("/");
        }
        else cert_directory = "ssl_certs/";

        hello_world_direct hwd(url);
        proton::container(hwd).run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}


bool using_OpenSSL() {
    // Current defaults.
#if defined(WIN32)
    return false;
#else
    return true;
#endif
}

ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd) {
    if (using_OpenSSL()) {
        // The first argument will be the name of the file containing the public certificate, the
        // second argument will be the name of the file containing the private key.
        return ssl_certificate(cert_directory + base_name + "-certificate.pem",
                               cert_directory + base_name + "-private-key.pem", passwd);
    }
    else {
        // Windows SChannel
        // The first argument will be the database or store that contains one or more complete certificates
        // (public and private data).  The second will be an optional name of the certificate in the store
        // (not used in this example with one certificate per store).
        return ssl_certificate(cert_directory + base_name + "-full.p12", "", passwd);
    }
}

std::string platform_CA(const std::string &base_name) {
    if (using_OpenSSL()) {
        // In this simple example with self-signed certificates, the peer's certificate is the CA database.
        return cert_directory + base_name + "-certificate.pem";
    }
    else {
        // Windows SChannel.  Use a pkcs#12 file with just the peer's public certificate information.
        return cert_directory + base_name + "-certificate.p12";
    }
}
