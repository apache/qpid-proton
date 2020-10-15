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

set(testdata "${CMAKE_CURRENT_BINARY_DIR}/testdata")
set(test_env "")

# SASL configuration for tests
if(CyrusSASL_Saslpasswd_EXECUTABLE)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/testdata/sasl-conf/proton-server.conf.in"
    "${testdata}/sasl-conf/proton-server.conf")
  execute_process(
    COMMAND echo password
    COMMAND ${CyrusSASL_Saslpasswd_EXECUTABLE} -c -p -f "${testdata}/sasl-conf/proton.sasldb" -u proton user
    RESULT_VARIABLE ret)
  if(ret)
    message(WARNING "${CyrusSASL_Saslpasswd_EXECUTABLE}: error ${ret} - some SASL tests will be skipped")
  else()
    set(test_env ${test_env} "PN_SASL_CONFIG_PATH=${testdata}/sasl-conf")
  endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(test_env ${test_env} "PATH=$<TARGET_FILE_DIR:qpid-proton>")
endif()

macro(add_cpp_test test)
  add_executable (${test} src/${test}.cpp)
  target_link_libraries (${test} qpid-proton-cpp ${PLATFORM_LIBS})
  pn_add_test(
    EXECUTABLE
    NAME cpp-${test}
    APPEND_ENVIRONMENT ${test_env}
    COMMAND $<TARGET_FILE:${test}>
    ${ARGN})
endmacro(add_cpp_test)

add_cpp_test(codec_test)
add_cpp_test(connection_driver_test)
add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)
add_cpp_test(message_test)
add_cpp_test(map_test)
add_cpp_test(scalar_test)
add_cpp_test(value_test)
add_cpp_test(container_test)
add_cpp_test(reconnect_test)
add_cpp_test(link_test)
add_cpp_test(credit_test)
if (ENABLE_JSONCPP)
  add_cpp_test(connect_config_test)
  target_link_libraries(connect_config_test qpid-proton-core) # For pn_sasl_enabled
  set_tests_properties(cpp-connect_config_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  # Test data and output directories for connect_config_test
  file(COPY  "${CMAKE_CURRENT_SOURCE_DIR}/testdata" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# TODO aconway 2018-10-31: Catch2 tests
# This is a simple example of a C++ test using the Catch2 framework.
# See c/tests/ for more interesting examples.
# Eventually all the C++ tests will migrate to Catch2.

include_directories(${CMAKE_SOURCE_DIR}/tests/include)
add_executable(cpp-test src/cpp-test.cpp src/url_test.cpp)
target_link_libraries(cpp-test qpid-proton-cpp ${PLATFORM_LIBS})
# tests that require access to pn_ functions in qpid-proton-core
add_executable(cpp-core-test src/cpp-test.cpp src/object_test.cpp)
target_link_libraries(cpp-core-test qpid-proton-cpp qpid-proton-core ${PLATFORM_LIBS})

macro(add_catch_test tag)
  pn_add_test(
    EXECUTABLE
    NAME cpp-${tag}-test
    APPEND_ENVIRONMENT ${test_env}
    COMMAND $<TARGET_FILE:cpp-test> "[${tag}]")
  set_tests_properties(cpp-${tag}-test PROPERTIES  FAIL_REGULAR_EXPRESSION ".*No tests ran.*")
endmacro(add_catch_test)

macro(add_core_catch_test tag)
  pn_add_test(
          EXECUTABLE
          NAME cpp-${tag}-test
          APPEND_ENVIRONMENT ${test_env}
          COMMAND $<TARGET_FILE:cpp-core-test> "[${tag}]")
  set_tests_properties(cpp-${tag}-test PROPERTIES  FAIL_REGULAR_EXPRESSION ".*No tests ran.*")
endmacro(add_core_catch_test)

add_catch_test(url)
add_core_catch_test(object)
