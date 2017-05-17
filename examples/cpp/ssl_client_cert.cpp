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

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/default_container.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/sasl.hpp>
#include <proton/ssl.hpp>
#include <proton/thread_safe.hpp>
#include <proton/tracker.hpp>
#include <proton/transport.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

using proton::connection_options;
using proton::ssl_client_options;
using proton::ssl_server_options;
using proton::ssl_certificate;
using proton::sasl;

// Helper functions defined below.
bool using_OpenSSL();
std::string platform_CA(const std::string &base_name);
ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd);
static std::string cert_directory;
static std::string find_CN(const std::string &);


struct server_handler : public proton::messaging_handler {
    proton::listener listener;

    void on_connection_open(proton::connection &c) OVERRIDE {
        std::cout << "Inbound server connection connected via SSL.  Protocol: " <<
            c.transport().ssl().protocol() << std::endl;
        if (c.transport().sasl().outcome() == sasl::OK) {
            std::string subject = c.transport().ssl().remote_subject();
            std::cout << "Inbound client certificate identity " << find_CN(subject) << std::endl;
        }
        else {
            std::cout << "Inbound client authentication failed" <<std::endl;
            c.close();
        }
        listener.stop();

        // Go and do default inbound open stuff too
        messaging_handler::on_connection_open(c);
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << m.body() << std::endl;
    }
};


class hello_world_direct : public proton::messaging_handler {
  private:
    std::string url;
    server_handler s_handler;

  public:
    hello_world_direct(const std::string& u) : url(u) {}

    void on_container_start(proton::container &c) OVERRIDE {
        // Configure listener.  Details vary by platform.
        ssl_certificate server_cert = platform_certificate("tserver", "tserverpw");
        std::string client_CA = platform_CA("tclient");
        // Specify an SSL domain with CA's for client certificate verification.
        ssl_server_options srv_ssl(server_cert, client_CA);
        connection_options server_opts;
        server_opts.ssl_server_options(srv_ssl).handler(s_handler);
        server_opts.sasl_allowed_mechs("EXTERNAL");
        c.server_connection_options(server_opts);

        // Configure client.
        ssl_certificate client_cert = platform_certificate("tclient", "tclientpw");
        std::string server_CA = platform_CA("tserver");
        // Since the test certifcate's credentials are unlikely to match this host's name, downgrade the verification
        // from VERIFY_PEER_NAME to VERIFY_PEER.
        ssl_client_options ssl_cli(client_cert, server_CA, proton::ssl::VERIFY_PEER);
        connection_options client_opts;
        client_opts.ssl_client_options(ssl_cli).sasl_allowed_mechs("EXTERNAL");
        c.client_connection_options(client_opts);

        s_handler.listener = c.listen(url);
        c.open_sender(url);
    }

    void on_connection_open(proton::connection &c) OVERRIDE {
        std::string subject = c.transport().ssl().remote_subject();
        std::cout << "Outgoing client connection connected via SSL.  Server certificate identity " <<
            find_CN(subject) << std::endl;
    }

    void on_sendable(proton::sender &s) OVERRIDE {
        proton::message m;
        m.body("Hello World!");
        s.send(m);
        s.close();
    }

    void on_tracker_accept(proton::tracker &t) OVERRIDE {
        // All done.
        t.connection().close();
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
        proton::default_container(hwd).run();
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

std::string find_CN(const std::string &subject) {
    // The subject string is returned with different whitespace and component ordering between platforms.
    // Here we just return the common name by searching for "CN=...." in the subject, knowing that
    // the test certificates do not contain any escaped characters.
    size_t pos = subject.find("CN=");
    if (pos == std::string::npos) throw std::runtime_error("No common name in certificate subject");
    std::string cn = subject.substr(pos);
    pos = cn.find(',');
    return pos == std::string::npos ? cn : cn.substr(0, pos);
}
