#ifndef PROTON_TRACING_HPP
#define PROTON_TRACING_HPP

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

#include "./internal/export.hpp"
#include <memory>

#ifdef ENABLE_OPENTELEMETRYCPP
    #include <opentelemetry/exporters/ostream/span_exporter.h>
    #include <opentelemetry/sdk/trace/simple_processor.h>
    #include <opentelemetry/sdk/trace/tracer_provider.h>
    #include <opentelemetry/trace/provider.h>
    #include <opentelemetry/trace/span.h>
    #include "opentelemetry/trace/tracer.h"
    #include <opentelemetry/trace/span.h>
#endif

#include <proton/delivery.hpp>

/// @file
/// @copybrief proton::tracing

namespace proton {
#ifdef ENABLE_OPENTELEMETRYCPP
    namespace trace = opentelemetry::trace;
    namespace sdktrace = opentelemetry::sdk::trace;

    // Tracer initializer.
    PN_CPP_EXTERN void initTracer(std::unique_ptr<sdktrace::SpanExporter> exporter);
#endif

// Span function.
PN_CPP_EXTERN void on_message_start_span(message &message, delivery &d);
PN_CPP_EXTERN void on_message_end_span(delivery &d);
PN_CPP_EXTERN void on_settled_span(delivery &d, tracker &track);
PN_CPP_EXTERN void send_span(const message &message, binary &tag, tracker &track);

} // namespace proton

#endif // PROTON_TRACING_HPP
