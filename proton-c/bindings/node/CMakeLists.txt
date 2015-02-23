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
cmake_minimum_required(VERSION 2.6)

if (NOT BUILD_WITH_CXX)
  message(FATAL_ERROR "ERROR: node bindings require CXX build to be enabled.\n"
    "Please set BUILD_WITH_CXX before rebuilding.")
endif()

find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# NB: *may* require cmake 3.1 or newer to successfully detect swig3.0 binary
#     ahead of swig2.0 binary (see commit 7400695 in kitware/cmake)
if (${SWIG_VERSION} VERSION_LESS 3.0)
  message(FATAL_ERROR "ERROR: swig-${SWIG_VERSION} does not support generation"
    " of Node.js bindings.\nPlease upgrade to swig-3.0 or newer to build these"
  )
endif()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/proton/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/../../include/proton/version.h"
)
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/binding.gyp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/binding.gyp"
)

include_directories("${CMAKE_CURRENT_BINARY_DIR}/../../src")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/../../include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../include")

find_path (NODE_ROOT_DIR "node/node.h" "src/node.h" HINTS /usr/include/nodejs)
# message("NODE_ROOT_DIR=${NODE_ROOT_DIR}")
include_directories("${NODE_ROOT_DIR}/src")
include_directories("${NODE_ROOT_DIR}/deps/v8/include")
include_directories("${NODE_ROOT_DIR}/deps/uv/include")

set(CMAKE_CXX_CREATE_SHARED_MODULE ${CMAKE_CXX_CREATE_SHARED_LIBRARY})
set(CMAKE_SWIG_FLAGS "-node;-I${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set_source_files_properties(javascript.i PROPERTIES CPLUSPLUS ON)
swig_add_module(cproton javascript javascript.i)
set_target_properties (cproton PROPERTIES LINKER_LANGUAGE CXX)
list(APPEND SWIG_MODULE_cproton_javascript_EXTRA_DEPS
  ${CMAKE_CURRENT_SOURCE_DIR}/../../proton-c/include/proton/cproton.i
  ${PROTON_HEADERS}
)
set_target_properties (cproton PROPERTIES
  COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -DBUILDING_NODE_EXTENSION"
  PREFIX ""
  SUFFIX ".node"
)
swig_link_libraries(cproton qpid-proton)
