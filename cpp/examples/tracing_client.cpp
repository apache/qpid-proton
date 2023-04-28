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
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source_options.hpp>
#include <proton/tracing.hpp>
#include <proton/tracker.hpp>
#include <proton/message_id.hpp>

#include <bits/stdc++.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

// Include opentelemetry header files
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/context.h>

using proton::receiver_options;
using proton::source_options;

opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> provider;
std::map<proton::message_id, std::shared_ptr<opentelemetry::trace::Scope>> scope_map;

int id_counter = 0;

class client : public proton::messaging_handler {
  private:
    std::string url;
    std::vector<std::string> requests;
    proton::sender sender;
    proton::receiver receiver;

  public:
    client(const std::string &u, const std::vector<std::string>& r) : url(u), requests(r) {}

    void on_container_start(proton::container &c) override {
        sender = c.open_sender(url);
        // Create a receiver requesting a dynamically created queue
        // for the message source.
        receiver_options opts = receiver_options().source(source_options().dynamic(true));
        receiver = sender.connection().open_receiver("", opts);
    }

    void send_request() {
        proton::message req;
        req.body(requests.front());
        req.reply_to(receiver.source().address());
        req.id(id_counter);

        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kClient;

        // Start a span here before send
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = provider->GetTracer("qpid-tracer", OPENTELEMETRY_SDK_VERSION);
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span = tracer->StartSpan("send_request",
            {{"reply_to", req.reply_to()}, {"message", to_string(req.body())}},
            options);
        opentelemetry::trace::Scope scope = tracer->WithActiveSpan(span);

        // Storing the 'scope' in a map to keep it alive and erasing it when a response is received.
        scope_map[req.id()] = std::make_shared<opentelemetry::trace::Scope>(std::move(scope));
        id_counter++;

        sender.send(req);
    }

    void on_receiver_open(proton::receiver &) override {
        send_request();
    }

    void on_message(proton::delivery &d, proton::message &response) override {
        if (requests.empty()) return; // Spurious extra message!

        // Converting the tag in proton::binary to std::string to add it as a span attribute. Tag in binary won't be visible.
        proton::binary tag = d.tag();
        std::string tag_in_string = std::string(tag);
        std::stringstream ss;
        for (int i = 0; i < (int)tag_in_string.length(); ++i)
            ss << std::hex << (int)tag[i];
        std::string delivery_tag = ss.str();

        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kClient;

        // Get Tracer
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = provider->GetTracer("qpid-tracer", OPENTELEMETRY_SDK_VERSION);

        // Start span with or without attributes as required.
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> s = tracer->StartSpan("on_message",
            {{"delivery_tag", delivery_tag}, {"message-received", to_string(response.body())}},
            options);

        // Mark span as active.
        opentelemetry::trace::Scope sc = tracer->WithActiveSpan(s);

        // Response has been received, thus erasing the 'scope' of the trace.
        scope_map.erase(response.id());

        std::cout << requests.front() << " => " << response.body() << std::endl;
        requests.erase(requests.begin());

        if (!requests.empty()) {
            send_request();
        } else {
            d.connection().close();
        }
    }
};

int main(int argc, char **argv) {
    try {
        std::string url("127.0.0.1:5672/examples");

        // 1. Initialize the exporter and the provider.
        // 2. Set the global trace provider.
        // 3. Call proton::initOpenTelemetryTracer().

        // Initialize otlp http Exporter
        opentelemetry::exporter::otlp::OtlpHttpExporterOptions options;
        options.url = "localhost:4318";
        auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(new opentelemetry::exporter::otlp::OtlpHttpExporter(options));

        // Set service-name
        auto resource_attributes = opentelemetry::sdk::resource::ResourceAttributes
        {
            {"service.name", "qpid-example-client"}
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

        // Sending 2 messages to the server.
        std::vector<std::string> requests;
        requests.push_back("Two roads diverged in a wood.");
        requests.push_back("I took the one less traveled by.");

        client c(url, requests);

        proton::container(c).run();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
