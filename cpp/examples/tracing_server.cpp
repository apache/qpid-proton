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
#include <proton/connection.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>

#include <proton/tracing.hpp>

#include <iostream>
#include <map>
#include <string>
#include <cctype>

// Include opentelemetry header files
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/exporters/jaeger/jaeger_exporter.h>
#include <opentelemetry/exporters/ostream/span_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/context.h>

opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> provider;

class server : public proton::messaging_handler {
    std::string conn_url_;
    std::string addr_;
    proton::connection conn_;
    std::map<std::string, proton::sender> senders_;

  public:
    server(const std::string& u, const std::string& a) :
        conn_url_(u), addr_(a) {}

    void on_container_start(proton::container& c) override {
        conn_ = c.connect(conn_url_);
        conn_.open_receiver(addr_);

        std::cout << "Server connected to " << conn_url_ << std::endl;
    }

    std::string to_upper(const std::string& s) {
        std::string uc(s);
        size_t l = uc.size();

        for (size_t i=0; i<l; i++) {
            uc[i] = static_cast<char>(std::toupper(uc[i]));
        }

        return uc;
    }

    void on_message(proton::delivery&, proton::message& m) override {

        // Start a span
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = provider->GetTracer("qpid-tracer", OPENTELEMETRY_SDK_VERSION);
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> s = tracer->StartSpan("on_message");

        opentelemetry::trace::Scope sc = tracer->WithActiveSpan(s);

        std::cout << "Received " << m.body() << std::endl;

        std::string reply_to = m.reply_to();
        proton::message reply;

        reply.to(reply_to);
        reply.body(to_upper(proton::get<std::string>(m.body())));
        reply.correlation_id(m.correlation_id());
        reply.id(m.id());

        if (!senders_[reply_to]) {
            senders_[reply_to] = conn_.open_sender(reply_to);
        }

        senders_[reply_to].send(reply);
    }
};

int main(int argc, char **argv) {
    try {
        std::string conn_url = argc > 1 ? argv[1] : "//127.0.0.1:5672";
        std::string addr = argc > 2 ? argv[2] : "examples";

        // 1. Initialize the exporter and the provider
        // 2. Set the global trace provider
        // 3. Call proton::initOpenTelemetryTracer()

        // Initialize Jaeger Exporter
        opentelemetry::exporter::jaeger::JaegerExporterOptions opts;
        std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(
            new opentelemetry::exporter::jaeger::JaegerExporter(opts));

        // Set service-name
        auto resource_attributes = opentelemetry::sdk::resource::ResourceAttributes
        {
            {"service.name", "qpid-example-server"}
        };

        // Creation of the resource for associating it with telemetry
        auto resource = opentelemetry::sdk::resource::Resource::Create(resource_attributes);

        auto processor = std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>(
            new opentelemetry::sdk::trace::SimpleSpanProcessor(std::move(exporter)));
        provider = opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
            new opentelemetry::sdk::trace::TracerProvider(std::move(processor), resource));

        // Set the global trace provider
        opentelemetry::trace::Provider::SetTracerProvider(provider);
    
        // Enable tracing in proton cpp
        proton::initOpenTelemetryTracer();

        server srv(conn_url, addr);
        proton::container(srv).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
