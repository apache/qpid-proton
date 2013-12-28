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

cmake_minimum_required (VERSION 2.6)

find_path(PROTON_INCLUDE_DIR proton/types.h)
find_library(PROTON_LIBRARY
             NAMES qpid-proton)

add_executable(recv recv.c)
add_executable(send send.c)
add_executable(recv-async recv-async.c)
add_executable(send-async send-async.c)

target_link_libraries(recv qpid-proton)
target_link_libraries(send qpid-proton)
target_link_libraries(recv-async qpid-proton)
target_link_libraries(send-async qpid-proton)

set_target_properties (
  recv send recv-async send-async
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS}"
)

if (BUILD_WITH_CXX)
  set_source_files_properties (recv.c send.c recv-async.c send-async.c PROPERTIES LANGUAGE CXX)
endif (BUILD_WITH_CXX)
