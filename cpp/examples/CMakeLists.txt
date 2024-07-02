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
cmake_minimum_required (VERSION 3.16)

project(ProtonCppExamples C CXX)

find_package(ProtonCpp REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-threaded examples
foreach(example
    broker
    helloworld
    simple_connect
    simple_recv
    simple_send
    reconnect_client
    message_properties
    scheduled_send_03
    direct_recv
    direct_send
    client
    server
    server_direct
    connection_options
    queue_browser
    colour_send
    selected_recv
    flow_control
    ssl
    ssl_client_cert
    encode_decode
    scheduled_send
    service_bus
    multithreaded_client
    multithreaded_client_flow_control)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} Proton::cpp)
endforeach()

# Tracing examples
if (ENABLE_OPENTELEMETRYCPP)
  foreach(example tracing_client tracing_server)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} Proton::cpp opentelemetry-cpp::trace opentelemetry-cpp::otlp_http_exporter)
  endforeach()
endif()
