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

include(CheckCXXSourceCompiles)

# Check compiler capabilities.
set(CMAKE_REQUIRED_FLAGS "${CXX_WARNING_FLAGS}")
check_cxx_source_compiles("long long ll; int main(int, char**) { return 0; }" HAVE_LONG_LONG)
if (HAVE_LONG_LONG)
  add_definitions(-DHAVE_LONG_LONG=1)
endif()

include_directories(
  "${CMAKE_SOURCE_DIR}/proton-c/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(qpid-proton-cpp-source
  src/acceptor.cpp
  src/acking.cpp
  src/connection.cpp
  src/connection_impl.cpp
  src/connector.cpp
  src/container.cpp
  src/container_impl.cpp
  src/data.cpp
  src/decoder.cpp
  src/delivery.cpp
  src/duration.cpp
  src/encoder.cpp
  src/endpoint.cpp
  src/error.cpp
  src/event.cpp
  src/handler.cpp
  src/link.cpp
  src/message.cpp
  src/messaging_adapter.cpp
  src/messaging_event.cpp
  src/messaging_handler.cpp
  src/proton_event.cpp
  src/proton_handler.cpp
  src/receiver.cpp
  src/sender.cpp
  src/session.cpp
  src/terminus.cpp
  src/transport.cpp
  src/url.cpp
  src/value.cpp
  src/values.cpp
  src/proton_bits.cpp
  src/blocking_connection.cpp
  src/blocking_connection_impl.cpp
  src/blocking_link.cpp
  src/blocking_sender.cpp
  src/contexts.cpp
  src/types.cpp
  )

set_source_files_properties (
  ${qpid-proton-cpp-source}
  PROPERTIES
  COMPILE_FLAGS "${CXX_WARNING_FLAGS}"
  )

add_library(qpid-proton-cpp SHARED ${qpid-proton-cpp-source})

target_link_libraries (qpid-proton-cpp ${PLATFORM_LIBS} qpid-proton)

set_target_properties (
  qpid-proton-cpp
  PROPERTIES
  LINKER_LANGUAGE CXX
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED}"
  )

## Test
if (ENABLE_VALGRIND AND VALGRIND_EXE)
  set(memcheck-cmd ${VALGRIND_EXE} --error-exitcode=1 --quiet --leak-check=full --trace-children=yes)
endif ()

macro(add_cpp_test test)
  add_executable (${test} src/${test}.cpp)
  target_link_libraries (${test} qpid-proton qpid-proton-cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_test (NAME cpp_${test}
      COMMAND ${env_py}
      "PATH=$<TARGET_FILE_DIR:qpid-proton>"
      ${test}> ${ARGN})
  else ()
    add_test (NAME cpp_${test} COMMAND ${memcheck-cmd} ${test} ${ARGN})
  endif ()
endmacro(add_cpp_test)

add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)

## Install

install(TARGETS qpid-proton-cpp
  EXPORT  proton
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install windows qpid-proton-cpp pdb files
if (MSVC)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/qpid-proton${CMAKE_DEBUG_POSTFIX}.pdb
    DESTINATION bin
    CONFIGURATIONS Debug
    OPTIONAL)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/qpid-proton.pdb
    DESTINATION bin
    CONFIGURATIONS RelWithDebInfo
    OPTIONAL)
endif (MSVC)

# Install header files
file(GLOB headers "include/proton/cpp/*.h")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton/cpp)

add_subdirectory(docs)
