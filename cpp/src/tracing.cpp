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

#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include "opentelemetry/trace/context.h"
// #include "opentelemetry/trace/experimental_semantic_conventions.h"

#include <proton/messaging_handler.hpp>

// Using an exporter that simply dumps span data to stdout.
#include <opentelemetry/exporters/ostream/span_exporter.h>
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

#include "opentelemetry/nostd/unique_ptr.h"

#include "opentelemetry/baggage/propagation/baggage_propagator.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"

#include <iostream>
#include <sstream>
#include <memory>

namespace proton
{
namespace trace = opentelemetry::trace;
namespace nostd = opentelemetry::nostd;
namespace common = opentelemetry::common;
namespace context = opentelemetry::context;

const std::string kContextKey = "CONTEXT";

// TODO: Delete map entries to avoid memory leakage in future
std::map<binary, nostd::shared_ptr<trace::Span>> tag_span;

template <typename T>
class AMQPTextMapCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
  public:
    AMQPTextMapCarrier<T>(T *message_annotations)
        : message_annotations_(message_annotations) {}
    virtual nostd::string_view
    Get(nostd::string_view key) const noexcept override {
        std::string key_to_compare = key.data();

        if (message_annotations_->exists(annotation_key(key_to_compare))) {
            value extracted_value =
                message_annotations_->get(annotation_key(key_to_compare));
            std::string extracted_string = to_string(extracted_value);
            extracted_strings.push_back(extracted_string);
            nostd::string_view final_extracted_string =
                nostd::string_view(extracted_strings.back());

            return final_extracted_string;
        }
        return "";
    }

    virtual void Set(nostd::string_view key,
                     nostd::string_view val) noexcept override {

        message_annotations_->put(annotation_key(std::string(key)),
                                  value(std::string(val)));
    }

    T *message_annotations_;
    mutable std::vector<std::string> extracted_strings;
};

void initTracer(std::unique_ptr<sdktrace::SpanExporter> exporter)
{
  // auto exporter = std::unique_ptr<sdktrace::SpanExporter>(
  //     new opentelemetry::exporter::trace::OStreamSpanExporter);
  auto processor = std::unique_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(exporter)));
  auto provider = nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
      new sdktrace::TracerProvider(std::move(processor)));

  // Set the global trace provider
  opentelemetry::trace::Provider::SetTracerProvider(provider);

  // set global propagator
  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

nostd::shared_ptr<trace::Tracer> get_tracer()
{
  auto provider = trace::Provider::GetTracerProvider();
  nostd::shared_ptr<opentelemetry::trace::Tracer> tracer =
      provider->GetTracer("qpid-tracer");
  return tracer;
}

void on_message_start_span(message &message, delivery &d) {

    opentelemetry::trace::StartSpanOptions options;
    options.kind          = opentelemetry::trace::SpanKind::kConsumer;

    // extract context from AMQP message annotations
    const AMQPTextMapCarrier<proton::message::annotation_map> carrier(
        &message.message_annotations());

    nostd::shared_ptr<
        opentelemetry::context::propagation::TextMapPropagator>
        prop = opentelemetry::context::propagation::GlobalTextMapPropagator::
            GetGlobalPropagator();
    context::Context current_ctx =
        opentelemetry::context::RuntimeContext::GetCurrent();

    context::Context new_context = prop->Extract(carrier, current_ctx);

    options.parent =
        opentelemetry::trace::GetSpan(new_context)->GetContext();

    binary tag_in_binary = d.tag();
    std::string tag_in_string = std::string(d.tag());
    std::stringstream ss;
    for(int i=0; i<(int)tag_in_string.length(); ++i)
        ss << std::hex << (int)tag_in_binary[i];
    std::string delivery_tag = ss.str();

    receiver r = d.receiver();
    source s = r.source();
    std::string s_addr = s.address();

    transfer tt(d);
    std::string delivery_state = to_string(tt.state());

    nostd::shared_ptr<trace::Span> span =
        get_tracer()->StartSpan("amqp-message-received",
                                {{"Delivery_tag", delivery_tag},
                                 {"Source_address", s_addr}},
                                options);

    trace::Scope scope = get_tracer()->WithActiveSpan(span);

    tag_span[tag_in_binary] = span;
}

void on_message_end_span(delivery &d) {
    binary tag = d.tag();
    nostd::shared_ptr<trace::Span> span = tag_span[tag];
    span->End();
}

void on_settled_span(delivery &d, tracker &track) {
    binary tag = d.tag();
    nostd::shared_ptr<trace::Span> span = tag_span[tag];
    transfer tt(track);
    std::string delivery_state = to_string(tt.state());
    span->AddEvent("delivery state: "+ delivery_state);
    span->End();
}

void send_span(const message &message, binary &tag, tracker &track) {

    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kProducer;

    binary tag_in_binary = tag;
    std::string tag_in_string = std::string(tag);
    std::stringstream ss;
    for (int i = 0; i < (int)tag_in_string.length(); ++i)
        ss << std::hex << (int)tag_in_binary[i];
    std::string delivery_tag = ss.str();

    sender s = track.sender();
    target t = s.target();
    std::string t_addr = t.address();

    transfer tt(track);
    std::string delivery_state = to_string(tt.state());

    nostd::shared_ptr<trace::Span> span = get_tracer()->StartSpan(
        "amqp-delivery-send",
        {{"Delivery_tag", delivery_tag}, {"Destination_address", t_addr}},
        options);

    trace::Scope scope = proton::get_tracer()->WithActiveSpan(span);

    // inject current context into AMQP message annotations
    context::Context current_ctx =
        opentelemetry::context::RuntimeContext::GetCurrent();
    AMQPTextMapCarrier<proton::message::annotation_map> carrier(
        &message.message_annotations());
    nostd::shared_ptr<
        opentelemetry::context::propagation::TextMapPropagator>
        prop = opentelemetry::context::propagation::GlobalTextMapPropagator::
            GetGlobalPropagator();
    prop->Inject(carrier, current_ctx);

    tag_span[tag_in_binary] = span;
}
}  // namespace proton
