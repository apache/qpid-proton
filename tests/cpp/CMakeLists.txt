#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

# Transaction tester: C++ binary plus script-driven CTest. Included from the
# top-level CMakeLists.txt after all bindings are configured.

if (NOT TARGET qpid-proton-cpp)
  return()
endif()

# Same proton C/C++ headers as cpp/ (tests/cpp is outside that subtree).
include_directories(
  "${PROJECT_SOURCE_DIR}/cpp/include"
  "${PROJECT_SOURCE_DIR}/c/include")

add_executable(tx_tester tx_tester.cpp)
target_link_libraries(tx_tester qpid-proton-cpp qpid-proton-core Threads::Threads)
target_include_directories(tx_tester PRIVATE ${PROJECT_SOURCE_DIR}/cpp/examples)

if(WIN32)
  # NOTE: need to escape semicolons as cmake uses them as list separators.
  set(test_path "$<TARGET_FILE_DIR:qpid-proton-cpp>\;$<TARGET_FILE_DIR:qpid-proton-core>")
else()
  set(test_path "$ENV{PATH}")
endif()

if (PYTHON_TEST_EXECUTABLE)
  pn_add_test(
    INTERPRETED
    NAME cpp-tx-tests
    PREPEND_ENVIRONMENT
      "PATH=${test_path}"
      "TX_TESTER=$<TARGET_FILE:tx_tester>"
      "TX_TESTER_PYTHON=${PYTHON_TEST_EXECUTABLE}"
      "TX_TESTER_BROKER=${PROJECT_SOURCE_DIR}/python/examples/broker.py"
      "TX_TESTER_SCRIPTS=${CMAKE_CURRENT_SOURCE_DIR}/tx_tests"
    COMMAND
      ${PYTHON_TEST_EXECUTABLE}
        -m unittest discover -v -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "tx_tester.py")
  set_tests_properties(cpp-tx-tests PROPERTIES RESOURCE_LOCK amqp_5672)
endif()
