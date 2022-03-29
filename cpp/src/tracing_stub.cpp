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

#include <proton/annotation_key.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/tracing.hpp>

#include "tracing_private.hpp"

namespace proton
{

class StubTracing : public Tracing {
  public:
    void message_encode(const message& m, std::vector<char>& buf, const binary& tag, const tracker& track) override {
        m.encode(buf);
    }

    void on_message_handler(messaging_handler& h, delivery& d, message& message) override {
        h.on_message(d, message);
    }

    void on_settled_span(tracker& track) override {}
};

static StubTracing dummy;
Tracing* Tracing::the = &dummy;

} // namespace proton
