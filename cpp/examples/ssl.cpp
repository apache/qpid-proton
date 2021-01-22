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

#include "options.hpp"

#include <proton/connection_options.hpp>
#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/error_condition.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/ssl.hpp>
#include <proton/tracker.hpp>
#include <proton/transport.hpp>

#include <iostream>

#include "fake_cpp11.hpp"

using proton::connection_options;
using proton::ssl_client_options;
using proton::ssl_server_options;
using proton::ssl_certificate;

// Helper functions defined below.
bool using_OpenSSL();
std::string platform_CA(const std::string &base_name);
ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd);
std::string find_CN(const std::string &);

namespace {
    const std::string verify_full("full");  // Normal verification
    const std::string verify_noname("noname"); // Skip matching host name against the certificate
    const std::string verify_fail("fail");  // Force name mismatch failure
    std::string verify(verify_full);  // Default for example
    std::string cert_directory;
}


struct server_handler : public proton::messaging_handler {
    proton::listener listener;

    void on_connection_open(proton::connection &c) OVERRIDE {
        std::cout << "Inbound server connection connected via SSL.  Protocol: " <<
            c.transport().ssl().protocol() << std::endl;
        listener.stop();  // Just expecting the one connection.

        // Go and do default inbound open stuff too
        messaging_handler::on_connection_open(c);
    }

    void on_transport_error(proton::transport &t) OVERRIDE {
        listener.stop();
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        std::cout << m.body() << std::endl;
    }
};


class hello_world_direct : public proton::messaging_handler {
  private:
    class listener_open_handler : public proton::listen_handler {
        void on_open(proton::listener& l) OVERRIDE {
            std::ostringstream url;
            url << "//:" << l.port() << "/example"; // Connect to the actual port
            l.container().open_sender(url.str());
        }
    };

    listener_open_handler listen_handler;
    server_handler s_handler;

  public:
    void on_container_start(proton::container &c) OVERRIDE {
        // Configure listener.  Details vary by platform.
        ssl_certificate server_cert = platform_certificate("tserver", "tserverpw");
        ssl_server_options ssl_srv(server_cert);
        connection_options server_opts;
        server_opts.ssl_server_options(ssl_srv).handler(s_handler);
        c.server_connection_options(server_opts);

        // Configure client with a Certificate Authority database
        // populated with the server's self signed certificate.
        connection_options client_opts;
        if (verify == verify_full) {
            ssl_client_options ssl_cli(platform_CA("tserver"));
            client_opts.ssl_client_options(ssl_cli);
            // The next line is optional in normal use.  Since the
            // example uses IP addresses in the connection string, use
            // the virtual_host option to set the server host name
            // used for certificate verification:
            client_opts.virtual_host("test_server");
        } else if (verify == verify_noname) {
            // Downgrade the verification from VERIFY_PEER_NAME to VERIFY_PEER.
            ssl_client_options ssl_cli(platform_CA("tserver"), proton::ssl::VERIFY_PEER);
            client_opts.ssl_client_options(ssl_cli);
        } else if (verify == verify_fail) {
            ssl_client_options ssl_cli(platform_CA("tserver"));
            client_opts.ssl_client_options(ssl_cli);
            client_opts.virtual_host("wrong_name_for_server"); // Pick any name that doesn't match.
        } else throw std::logic_error("bad verify mode: " + verify);

        c.client_connection_options(client_opts);
        s_handler.listener = c.listen("//:0", listen_handler); // Listen on port 0 for a dynamic port
    }

    void on_connection_open(proton::connection &c) OVERRIDE {
        std::string subject = c.transport().ssl().remote_subject();
        std::cout << "Outgoing client connection connected via SSL.  Server certificate identity " <<
            find_CN(subject) << std::endl;
    }

    void on_transport_error(proton::transport &t) OVERRIDE {
        std::string err = t.error().what();
        if (verify == verify_fail && err.find("certificate") != std::string::npos) {
            std::cout << "Expected failure of connection with wrong peer name: " << err
                      << std::endl;
        } else {
            std::cout << "Unexpected transport error: " << err << std::endl;
        }
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
    example::options opts(argc, argv);
    opts.add_value(cert_directory, 'c', "cert_directory",
                   "directory containing SSL certificates and private key information", "CERTDIR");
    opts.add_value(verify, 'v', "verify", "verify type: \"minimum\", \"full\", \"fail\"", "VERIFY");

    try {
        opts.parse();

        size_t sz = cert_directory.size();
        if (sz) {
            if (cert_directory[sz -1] != '/') {
                cert_directory.append("/");
	    }
        } else {
	    cert_directory = "ssl-certs/";
	}

        if (verify != verify_noname && verify != verify_full && verify != verify_fail) {
            throw std::runtime_error("bad verify argument: " + verify);
	}

        hello_world_direct hwd;
        proton::container(hwd).run();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}


bool using_OpenSSL() {
    // Current defaults.
#if defined(_WIN32)
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
