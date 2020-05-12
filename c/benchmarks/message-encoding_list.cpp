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
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proton/connection_driver.h"
#include "proton/engine.h"
#include "proton/log.h"
#include "proton/message.h"

#include <benchmark/benchmark.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <wait.h>

const bool VERBOSE = false;
const bool ERRORS = true;

static void decode_message_buffer(pn_rwbytes_t data) {
  pn_message_t *m = pn_message();
  int err = pn_message_decode(m, data.start, data.size);
  if (!err) {
    /* Print the decoded message */
    pn_string_t *s = pn_string(NULL);
    pn_inspect(pn_message_body(m), s);
    if (VERBOSE) {
      printf("%s\n", pn_string_get(s));
      fflush(stdout);
    }
    pn_free(s);
    pn_message_free(m);
    free(data.start);
  } else {
    fprintf(stderr, "decode error: %s\n", pn_error_text(pn_message_error(m)));
    exit(EXIT_FAILURE);
  }
}

static void BM_EncodeListMessage(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_EncodeListMessage\n");

  uint64_t entries = 0;

  pn_message_t *message = pn_message();
  for (auto _ : state) {
    pn_message_clear(message);
    pn_data_t *body;
    body = pn_message_body(message);
    pn_data_put_list(body);
    pn_data_enter(body);
    for (size_t i = 0; i < state.range(0); i += 2) {
      pn_data_put_string(
          body, pn_bytes(sizeof("some list value") - 1, "some list value"));
      pn_data_put_int(body, 42);
      entries += 2;
    }
    pn_data_exit(body);
    pn_rwbytes_t buf{};
    if (pn_message_encode2(message, &buf) == 0) {
      state.SkipWithError(pn_error_text(pn_message_error(message)));
    }
    if (buf.start)
      free(buf.start);
  }
  pn_message_free(message);

  state.SetLabel("entries");
  state.SetItemsProcessed(entries);

  if (VERBOSE)
    printf("END BM_EncodeListMessage\n");
}

BENCHMARK(BM_EncodeListMessage)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->ArgName("entries")
    ->Unit(benchmark::kMillisecond);

static void BM_CreateListMessage(benchmark::State &state) {
  if (VERBOSE)
    printf("BEGIN BM_CreateListMessage\n");

  uint64_t entries = 0;

  pn_message_t *message = pn_message();
  pn_data_t *body;
  body = pn_message_body(message);
  pn_data_put_list(body);
  pn_data_enter(body);
  for (auto _ : state) {
    pn_data_put_string(
        body, pn_bytes(sizeof("some list value") - 1, "some list value"));
    pn_data_put_int(body, 42);
    entries += 2;
  }
  pn_data_exit(body);
  pn_message_free(message);

  state.SetLabel("entries");
  state.SetItemsProcessed(entries);

  if (VERBOSE)
    printf("END BM_CreateListMessage\n");
}

BENCHMARK(BM_CreateListMessage);
