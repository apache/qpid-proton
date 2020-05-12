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

#include <iostream>

#include <benchmark/benchmark.h>

#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/messaging_handler.hpp>

// PROTON-2137 [cpp] Performance regression found in 0.29.0
//  proton would spend long time initializing ssl when not needed

class handler : public proton::messaging_handler {
  std::string url;

public:
  explicit handler(const std::string &u) : url(u) {}

private:
  void on_container_start(proton::container &c) override {
    c.connect(url, proton::connection_options().sasl_enabled(false));
  }

  void on_connection_open(proton::connection &c) override { c.close(); }
};

// expected to run in < 1 ms, for me it is 160 us
static void BM_UnsuccessfulContainerRun(benchmark::State &state) {
  for (auto _ : state) {
    try {
      handler h("127.0.0.1:0"); // wrong port
      proton::container(h).run();
      state.SkipWithError("Exception expected");
    } catch (std::exception &e) {
      // expected
    }
  }
}

BENCHMARK(BM_UnsuccessfulContainerRun)->Unit(benchmark::kMicrosecond);
