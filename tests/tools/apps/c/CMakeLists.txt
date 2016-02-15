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

include(CheckIncludeFiles)

include_directories(${CMAKE_SOURCE_DIR}/examples/c/include)

CHECK_INCLUDE_FILES("inttypes.h" INTTYPES_AVAILABLE)
if (INTTYPES_AVAILABLE)
  list(APPEND PLATFORM_DEFINITIONS "USE_INTTYPES")
else (INTTYPES_AVAILABLE)
  if (CMAKE_COMPILER_IS_GNUCC)
    # since inttypes.h provides portable printf format macros
    set (COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wno-format")
  endif (CMAKE_COMPILER_IS_GNUCC)
endif (INTTYPES_AVAILABLE)

add_executable(msgr-recv msgr-recv.c msgr-common.c)
add_executable(msgr-send msgr-send.c msgr-common.c)
add_executable(reactor-recv reactor-recv.c msgr-common.c)
add_executable(reactor-send reactor-send.c msgr-common.c)

target_link_libraries(msgr-recv qpid-proton)
target_link_libraries(msgr-send qpid-proton)
target_link_libraries(reactor-recv qpid-proton)
target_link_libraries(reactor-send qpid-proton)

set_target_properties (
  msgr-recv msgr-send reactor-recv reactor-send
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS}"
  COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
)

if (BUILD_WITH_CXX)
  set_source_files_properties (msgr-recv.c msgr-send.c msgr-common.c reactor-recv.c reactor-send.c PROPERTIES LANGUAGE CXX)
endif (BUILD_WITH_CXX)
