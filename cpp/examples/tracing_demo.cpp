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
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/tracker.hpp>
#include <proton/tracing.hpp>

#include <iostream>
#include <sstream>
#include <thread>

#include <opentelemetry/exporters/jaeger/jaeger_exporter.h>

class tracing_demo : public proton::messaging_handler {
    std::string conn_url_;
    std::string addr_;

  public:
    tracing_demo(const std::string& u, const std::string& a) :
        conn_url_(u), addr_(a) {}

    void on_container_start(proton::container& c) override {
        c.connect(conn_url_);
    }

    void on_connection_open(proton::connection& c) override {
        c.open_receiver(addr_);
        c.open_sender(addr_);
    }

    void on_sendable(proton::sender &s) override {
        proton::message m("Hello Tracing World!");
        s.send(m);
        std::cout << "Message sent: " << m.body() << std::endl;
        // std::cout << "send message_annotations: " << m.message_annotations()
        //           << std::endl;
        s.close();
    }

    void on_tracker_accept(proton::tracker &t) override {
        std::cout << "all messages confirmed" << std::endl;
    }

    void on_message(proton::delivery &d, proton::message &m) override {
        std::cout << "Message received: " << m.body() << std::endl;
        if (m.message_annotations().empty()) {
            std::cout << "Message annotations is empty " << std::endl;
        } else {
            std::cout << "Message annotations is not empty " << std::endl;
            std::cout << "Receive message_annotations: "
                      << m.message_annotations() << std::endl;
        }
        d.connection().close();
    }

};

int main(int argc, char **argv) {
    try {
        std::string conn_url = argc > 1 ? argv[1] : "//127.0.0.1:5672";
        std::string addr = argc > 2 ? argv[2] : "examples";

        opentelemetry::exporter::jaeger::JaegerExporterOptions opts;
        auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(
            new opentelemetry::exporter::jaeger::JaegerExporter(opts));
        // auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(
        //     new opentelemetry::exporter::trace::OStreamSpanExporter());

        proton::initTracer(std::move(exporter));
        tracing_demo hw(conn_url, addr);
        proton::container(hw).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
