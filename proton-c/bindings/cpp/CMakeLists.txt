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

set(CPP_FLAGS ${CXX_WARNING_FLAGS})

## Check compiler capabilities.
macro(compiler_test RESULT_VAR NAME CODE)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${NAME} "${CODE}")
  try_compile(${RESULT_VAR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${NAME})
endmacro()

compiler_test(RESULT has_long_long.cpp "long long ll; int main(int, char**) { return 0; }")
if (RESULT)
  message("C++ compiler has long long")
  set(CPP_FLAGS "${CPP_FLAGS} -DHAS_LONG_LONG")
else()
  message("C++ compiler does not have long long")
endif()

include_directories(
  "${CMAKE_SOURCE_DIR}/proton-c/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(qpid-proton-cpp-source
  src/Acceptor.cpp
  src/Acking.cpp
  src/Connection.cpp
  src/ConnectionImpl.cpp
  src/Connector.cpp
  src/Container.cpp
  src/ContainerImpl.cpp
  src/Data.cpp
  src/Decoder.cpp
  src/Delivery.cpp
  src/Duration.cpp
  src/Encoder.cpp
  src/Endpoint.cpp
  src/Error.cpp
  src/Event.cpp
  src/Handler.cpp
  src/Link.cpp
  src/Message.cpp
  src/MessagingAdapter.cpp
  src/MessagingEvent.cpp
  src/MessagingHandler.cpp
  src/ProtonEvent.cpp
  src/ProtonHandler.cpp
  src/Receiver.cpp
  src/Sender.cpp
  src/Session.cpp
  src/Terminus.cpp
  src/Transport.cpp
  src/Url.cpp
  src/Value.cpp
  src/Values.cpp
  src/proton_bits.cpp
  src/BlockingConnection.cpp
  src/BlockingConnectionImpl.cpp
  src/BlockingLink.cpp
  src/BlockingSender.cpp
  src/contexts.cpp
  src/types.cpp
  )

set_source_files_properties (
  ${qpid-proton-cpp-source}
  PROPERTIES
  COMPILE_FLAGS "${CPP_FLAGS}"
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
