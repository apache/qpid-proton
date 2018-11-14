/*
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
 */

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

namespace Catch {

// Same as built-in "console" reporter, but prints the test name before
// running each test, as a progress indicator. Enable with
//     -r console.progress
struct ConsoleProgresReporter : ConsoleReporter {

  ConsoleProgresReporter(ReporterConfig const &config)
      : ConsoleReporter(config) {}

  static std::string getDescription() {
    return "Shows each test-case before running, results as plain lines of "
           "text";
  }

  void testCaseStarting(TestCaseInfo const &testCaseInfo) CATCH_OVERRIDE {
    stream << "running test case \"" << testCaseInfo.name << '"' << std::endl;
    ConsoleReporter::testCaseStarting(testCaseInfo);
  }
};

INTERNAL_CATCH_REGISTER_REPORTER("console.progress", ConsoleProgresReporter)

} // namespace Catch
