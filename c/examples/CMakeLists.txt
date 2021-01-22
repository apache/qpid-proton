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

project(ProtonCExamples C)

find_package(Proton REQUIRED Core Proactor)
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

foreach (name broker send receive direct send-abort send-ssl raw_echo raw_connect)
  add_executable(c-${name} ${name}.c)
  target_link_libraries(c-${name} Proton::core Proton::proactor Threads::Threads)
  set_target_properties(c-${name} PROPERTIES
    OUTPUT_NAME ${name})
endforeach()
