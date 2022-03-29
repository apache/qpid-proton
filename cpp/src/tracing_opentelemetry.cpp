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

#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

#include <proton/messaging_handler.hpp>

#include <proton/annotation_key.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/receiver.hpp>
#include <proton/sender.hpp>
#include <proton/source.hpp>
#include <proton/target.hpp>
#include <proton/tracing.hpp>
#include <proton/tracker.hpp>
#include <proton/transfer.hpp>

#include "proton/link.hpp"
#include <proton/link.h>

#include "tracing_private.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

// Custom specialization of std::hash injected in namespace std for proton::binary as a key in tag_span i.e. an unordered_map.
template <> struct std::hash<proton::binary> {
    std::size_t operator()(const proton::binary& k) const {
        std::string s(k[0], k.size());
        return std::hash<std::string>{}(s);
    }
};

namespace proton
{
namespace nostd = opentelemetry::nostd;
namespace sdktrace = opentelemetry::sdk::trace;

const std::string kContextKey = "x-opt-qpid-tracestate";

// TODO: Have a delivery context to do the work, instead of having a map to associate the spans with the delivery tags.

// A map to associate the spans with the delivery tags, needed for ending the spans on message settled.
// Deleting the map entries after the span is ended to avoid the memory leakage in future.
std::unordered_map<binary, nostd::shared_ptr<opentelemetry::trace::Span>> tag_span;

class AMQPMapCarrier : public opentelemetry::context::propagation::TextMapCarrier {
  public:
    AMQPMapCarrier(const proton::map<annotation_key, value>* message_annotations) : message_annotations_(message_annotations) {}
    virtual nostd::string_view Get(nostd::string_view key) const noexcept override {
        std::string key_to_compare = key.data();

        proton::value v_tracing_map = message_annotations_->get(annotation_key(kContextKey));
        proton::map<proton::annotation_key, proton::value> tracing_map;

        if (!v_tracing_map.empty())
            get(v_tracing_map, tracing_map);

        if (tracing_map.exists(annotation_key(key_to_compare))) {
            value extracted_value = tracing_map.get(annotation_key(key_to_compare));
            std::string extracted_string = to_string(extracted_value);
            extracted_strings.push_back(extracted_string);
            nostd::string_view final_extracted_string = nostd::string_view(extracted_strings.back());

            return final_extracted_string;
        }
        return "";
    }
    virtual void Set(nostd::string_view key,
                     nostd::string_view val) noexcept override {

        proton::value v_tracing_map = message_annotations_->get(annotation_key(kContextKey));
        proton::map<proton::annotation_key, proton::value> tracing_map;

        if (!v_tracing_map.empty())
            get(v_tracing_map, tracing_map);

        tracing_map.put(annotation_key(std::string(key)), value(std::string(val)));
        ((proton::map<proton::annotation_key, proton::value>*)message_annotations_)->put(annotation_key(kContextKey), tracing_map);
    }

    const proton::map<annotation_key, value>* message_annotations_;
    mutable std::vector<std::string> extracted_strings;
};

nostd::shared_ptr<opentelemetry::trace::Tracer> get_tracer() {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    nostd::shared_ptr<opentelemetry::trace::Tracer> tracer = provider->GetTracer("qpid-tracer", OPENTELEMETRY_SDK_VERSION);
    return tracer;
}

class OpentelemetryTracing : public Tracing {
  public:
    void message_encode(const message& message, std::vector<char>& buf, const binary& tag, const tracker& track) override {
        proton::message message_cp = message;

        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kProducer;

        opentelemetry::context::Context ctx = opentelemetry::context::RuntimeContext::GetCurrent();
        options.parent = opentelemetry::trace::GetSpan(ctx)->GetContext();

        std::string tag_in_string = std::string(tag);
        std::stringstream ss;
        for (int i = 0; i < (int)tag_in_string.length(); ++i)
            ss << std::hex << (int)tag[i];
        std::string delivery_tag = ss.str();

        sender s = track.sender();
        target t = s.target();
        std::string t_addr = t.address();

        std::string delivery_state = to_string(track.state());

        nostd::shared_ptr<opentelemetry::trace::Span> span = get_tracer()->StartSpan(
            "amqp-delivery-send",
            {{"Delivery_tag", delivery_tag}, {"Destination_address", t_addr}},
            options);

        opentelemetry::trace::Scope scope = proton::get_tracer()->WithActiveSpan(span);

        // Inject current context into AMQP message annotations
        opentelemetry::context::Context current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();

        AMQPMapCarrier carrier(&message_cp.message_annotations());
        nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator> prop =
            opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
        prop->Inject(carrier, current_ctx);

        tag_span[tag] = span;

        message_cp.encode(buf);
    }

    void on_message_handler(messaging_handler& h, delivery& d, message& message) override {
        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kConsumer;

        // Extract context from AMQP message annotations
        const AMQPMapCarrier carrier(&message.message_annotations());

        nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator> prop =
            opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
        opentelemetry::context::Context current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();

        opentelemetry::context::Context new_context = prop->Extract(carrier, current_ctx);

        options.parent = opentelemetry::trace::GetSpan(new_context)->GetContext();

        binary tag_in_binary = d.tag();
        std::string tag_in_string = std::string(d.tag());
        std::stringstream ss;
        for (int i = 0; i < (int)tag_in_string.length(); ++i)
            ss << std::hex << (int)tag_in_binary[i];
        std::string delivery_tag = ss.str();

        receiver r = d.receiver();
        source s = r.source();
        std::string s_addr = s.address();

        transfer tt(d);
        std::string delivery_state = to_string(tt.state());

        nostd::shared_ptr<opentelemetry::trace::Span> span = get_tracer()->StartSpan(
            "amqp-message-received",
            {{"Delivery_tag", delivery_tag}, {"Source_address", s_addr}},
            options);

        opentelemetry::trace::Scope scope = get_tracer()->WithActiveSpan(span);

        h.on_message(d, message);

        span->End();
    }

    void on_settled_span(tracker& track) override {

        binary tag = track.tag();
        nostd::shared_ptr<opentelemetry::trace::Span> span = tag_span[tag];
        std::string delivery_state = to_string(track.state());
        span->AddEvent("delivery state: " + delivery_state);

        span->End();

        // Delete map entries.
        tag_span.erase(tag);
    }
};

static OpentelemetryTracing otel;

void initOpenTelemetryTracer()
{  
    Tracing::activate(otel);

    // Set global propagator
    opentelemetry::context::propagation::GlobalTextMapPropagator::
        SetGlobalPropagator(
            nostd::shared_ptr<
                opentelemetry::context::propagation::TextMapPropagator>(
                new opentelemetry::trace::propagation::HttpTraceContext()));
}

}  // namespace proton
