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

#include "proton/message.h"

#include <benchmark/benchmark.h>
#include <core/data.h>
#include <proton/netaddr.h>

#include <cstdlib>
#include <cstring>

/** Initialize message, don't decode?
 * Issues:
 *   PROTON-2229 pn_data_t initialization lead to low performance
 */
static void BM_Encode10MbMessage(benchmark::State &state) {
    const size_t size = 10 * 1024 * 1024;
    char *payload = static_cast<char *>(malloc(size));
    pn_bytes_t bytes = pn_bytes(size, payload);

    for (auto _ : state) {
        pn_message_t *message = pn_message();
        pn_data_t *body = pn_message_body(message);
        for (size_t i = 0; i < state.range(0); i++) {
            pn_data_put_binary(body, bytes);
        }

        pn_message_free(message);
    }

    free(payload);
}

BENCHMARK(BM_Encode10MbMessage)
        ->Arg(1)
        ->Arg(2)
        ->ArgName("put_binary count")
        ->Unit(benchmark::kMillisecond);
