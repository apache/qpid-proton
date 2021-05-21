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
cmake_minimum_required (VERSION 2.8.12)

project(ProtonCppExamples C CXX)

find_package(ProtonCpp REQUIRED)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# The following block is purely for CMake 2.8.12
# Remove it when we no longer support CMake 2.8.12
if(NOT TARGET Threads::Threads)
  add_library(Threads::Threads UNKNOWN IMPORTED)

  if(THREADS_HAVE_PTHREAD_ARG)
    set_target_properties(Threads::Threads PROPERTIES
      INTERFACE_COMPILE_OPTIONS "-pthread"
    )
  endif()

  if(CMAKE_THREAD_LIBS_INIT)
    set_target_properties(Threads::Threads PROPERTIES
      IMPORTED_LOCATION "${CMAKE_THREAD_LIBS_INIT}"
    )
  endif()
endif()

# This effectively checks for cmake version 3.1 or later
if (DEFINED CMAKE_CXX_COMPILE_FEATURES)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_EXTENSIONS OFF)
else ()
  # This can only be a fairly old version of Linux in practice
  set(CXX_STANDARD "-std=c++11")
  add_definitions(${CXX_STANDARD})
endif()


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
    service_bus)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} Proton::cpp)
endforeach()

# Examples that use threads directly
foreach(example
    multithreaded_client
    multithreaded_client_flow_control)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} Proton::cpp Threads::Threads)
endforeach()
