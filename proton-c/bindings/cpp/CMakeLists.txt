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

include(cpp.cmake) # Compiler checks

include_directories(
  "${CMAKE_SOURCE_DIR}/proton-c/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(qpid-proton-cpp-source
  src/acceptor.cpp
  src/binary.cpp
  src/scalar.cpp
  src/condition.cpp
  src/connection.cpp
  src/connection_options.cpp
  src/connector.cpp
  src/container.cpp
  src/container_impl.cpp
  src/contexts.cpp
  src/data.cpp
  src/decoder.cpp
  src/decimal.cpp
  src/delivery.cpp
  src/duration.cpp
  src/encoder.cpp
  src/endpoint.cpp
  src/connection_engine.cpp
  src/error.cpp
  src/id_generator.cpp
  src/link.cpp
  src/link_options.cpp
  src/message.cpp
  src/messaging_adapter.cpp
  src/messaging_event.cpp
  src/handler.cpp
  src/object.cpp
  src/proton_bits.cpp
  src/proton_event.cpp
  src/proton_handler.cpp
  src/reactor.cpp
  src/receiver.cpp
  src/reconnect_timer.cpp
  src/sasl.cpp
  src/sender.cpp
  src/session.cpp
  src/ssl.cpp
  src/ssl_domain.cpp
  src/task.cpp
  src/terminus.cpp
  src/timestamp.cpp
  src/transport.cpp
  src/type_id.cpp
  src/url.cpp
  src/uuid.cpp
  src/value.cpp
  )

if(MSVC)
  list(APPEND qpid-proton-cpp-source src/windows/io.cpp)
else(MSVC)
  list(APPEND qpid-proton-cpp-source src/posix/io.cpp)
endif(MSVC)

set_source_files_properties (
  ${qpid-proton-cpp-source}
  PROPERTIES
  COMPILE_FLAGS "${CXX_WARNING_FLAGS} ${LTO}"
  )

add_library(qpid-proton-cpp SHARED ${qpid-proton-cpp-source})

target_link_libraries (qpid-proton-cpp ${PLATFORM_LIBS} qpid-proton)

set_target_properties (
  qpid-proton-cpp
  PROPERTIES
  LINKER_LANGUAGE CXX
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
  )

## Install

install(TARGETS qpid-proton-cpp
  EXPORT  proton-cpp
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install windows qpid-proton-cpp pdb files
if (MSVC)
  install(FILES $<TARGET_PDB_FILE:qpid-proton-cpp>
    DESTINATION bin
    CONFIGURATIONS RelWithDebInfo Debug
    OPTIONAL)
endif (MSVC)

# Install header files
file(GLOB headers "include/proton/*.hpp")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton)

add_subdirectory(docs)
add_subdirectory(${CMAKE_SOURCE_DIR}/tests/tools/apps/cpp ${CMAKE_BINARY_DIR}/tests/tools/apps/cpp)

# Pkg config file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libqpid-proton-cpp.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libqpid-proton-cpp.pc @ONLY)
install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libqpid-proton-cpp.pc
  DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

if (DEFINED CMAKE_IMPORT_LIBRARY_PREFIX)
set(PROTONCPPLIB ${CMAKE_IMPORT_LIBRARY_PREFIX}qpid-proton-cpp${CMAKE_IMPORT_LIBRARY_SUFFIX})
set(PROTONCPPLIBDEBUG ${CMAKE_IMPORT_LIBRARY_PREFIX}qpid-proton-cpp${CMAKE_DEBUG_POSTFIX}${CMAKE_IMPORT_LIBRARY_SUFFIX})
else ()
set(PROTONCPPLIB ${CMAKE_SHARED_LIBRARY_PREFIX}qpid-proton-cpp${CMAKE_SHARED_LIBRARY_SUFFIX})
set(PROTONCPPLIBDEBUG ${CMAKE_SHARED_LIBRARY_PREFIX}qpid-proton-cpp${CMAKE_DEBUG_POSTFIX}${CMAKE_SHARED_LIBRARY_SUFFIX})
endif ()

include(WriteBasicConfigVersionFile)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/ProtonCppConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonCppConfig.cmake @ONLY)
write_basic_config_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonCppConfigVersion.cmake
  VERSION ${PN_VERSION}
  COMPATIBILITY AnyNewerVersion)
install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonCppConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonCppConfigVersion.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/ProtonCpp)

## Test
if (ENABLE_VALGRIND AND VALGRIND_EXE)
  set(memcheck-cmd ${VALGRIND_EXE} --error-exitcode=42 --quiet --leak-check=full --trace-children=yes)
endif ()

macro(add_cpp_test test)
  add_executable (${test} src/${test}.cpp)
  target_link_libraries (${test} qpid-proton qpid-proton-cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_test (NAME cpp_${test}
      COMMAND ${env_py}
      "PATH=$<TARGET_FILE_DIR:qpid-proton>"
      $<TARGET_FILE:${test}> ${ARGN})
  else ()
    add_test (NAME cpp_${test} COMMAND ${memcheck-cmd} ${CMAKE_CURRENT_BINARY_DIR}/${test} ${ARGN})
  endif ()
endmacro(add_cpp_test)

add_cpp_test(codec_test)
add_cpp_test(engine_test)
add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)
add_cpp_test(message_test)
add_cpp_test(scalar_test)
add_cpp_test(value_test)
