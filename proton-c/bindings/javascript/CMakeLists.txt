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
 
# This file allows cross-compiling of proton to JavaScript using emscripten (https://github.com/kripken/emscripten)
# As it is really a cross-compilation (as opposed to a binding a la swig) the approach is rather different and
# somewhat replicates the main build in the proton-c/CMakeLists.txt using quite a bit of "copy and paste reuse".
# TODO refactor this file (and proton-c/CMakeLists.txt) to keep the main compilation and cross-compilation consistent.

message(STATUS "Found emscripten, using that to build JavaScript binding")

# Specify the compiler to use for cross-compilation.
set(CMAKE_C_COMPILER "${EMCC}")

# Specify the archiver to use for cross-compilation.
find_program(EMAR emar)
set(CMAKE_AR "${EMAR}")

# Specify the ranlib to use for cross-compilation.
find_program(EMRANLIB emranlib)
set(CMAKE_RANLIB "${EMRANLIB}")

# Don't do compiler autodetection, since we are cross-compiling.
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER("${CMAKE_C_COMPILER}" Clang)

# Specify the program to use when building static libraries. Force Emscripten-related command line options to clang.
set(CMAKE_C_ARCHIVE_CREATE "${CMAKE_AR} rc <TARGET> ${CMAKE_START_TEMP_FILE} <LINK_FLAGS> <OBJECTS>${CMAKE_END_TEMP_FILE}")

# From this point we should be using emscripten compilation tools rather than default ones
message(STATUS "emscripten compilation environment:")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_AR: ${CMAKE_AR}")
message(STATUS "CMAKE_RANLIB: ${CMAKE_RANLIB}")
message(STATUS "CMAKE_C_ARCHIVE_CREATE: ${CMAKE_C_ARCHIVE_CREATE}")

# Set this to the proton-c directory, we're cross-compiling code from there.
set(PN_PATH ${CMAKE_SOURCE_DIR}/proton-c)


# TODO the OpenSSL stuff won't work for emscripten by default. It might well be possible to compile it from
# source using emscripten (that's the standard practice for libraries with emscripten) see the comments in
# https://github.com/kripken/emscripten/wiki/FAQ "Q. How do I link against system libraries like SDL, boost, etc.?"
# Though to be honest it might be more natural to simply use a TLS protected wss:// WebSocket URL.
#  set(pn_driver_ssl_impl src/ssl/openssl.c)
#  set(SSL_LIB ${OPENSSL_LIBRARIES})
set(pn_driver_ssl_impl ${PN_PATH}/src/ssl/ssl_stub.c)
set(SSL_LIB "")

# emscripten is Linux like so use the posix driver.
set(pn_driver_impl ${PN_PATH}/src/posix/driver.c)


# Use stub UUID library (implemented in JavaScript)
# TODO push proper libuuid back to emscripten and remove this from here.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/uuid/include")
set(UUID_LIB "")
set(UUID_STUB_LIB ${CMAKE_CURRENT_SOURCE_DIR}/uuid/src/library_uuid.js)

# The time lib is in the standard emscripten libraries.
set(TIME_LIB "")

# Generate encodings.h and protocol.h
# It may be possible to use the ones generated for the main proton-c build but qpid-proton-core has
# a dependency on the generated files so I'm not sure if it'll work without a command that can
# be run as a dependency. Probably best do it this way when cross-compiling - though more copy'n'paste
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/encodings.h
  COMMAND python ${PN_PATH}/env.py PYTHONPATH=${PN_PATH} python ${PN_PATH}/src/codec/encodings.h.py > ${CMAKE_CURRENT_BINARY_DIR}/encodings.h
  DEPENDS ${PN_PATH}/src/codec/encodings.h.py
  )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/protocol.h
  COMMAND python ${PN_PATH}/env.py PYTHONPATH=${PN_PATH} python ${PN_PATH}/src/protocol.h.py > ${CMAKE_CURRENT_BINARY_DIR}/protocol.h
  DEPENDS ${PN_PATH}/src/protocol.h.py
  )

# TODO put back -Werror once offending warnings get eliminated.
#set(COMPILE_WARNING_FLAGS "-Werror -Wall -pedantic-errors -Wno-comment -Wno-warn-absolute-paths")
set(COMPILE_WARNING_FLAGS "-Wall -pedantic-errors -Wno-comment -Wno-warn-absolute-paths")

# Explicitly set PLATFORM_DEFINITIONS to Linux ones for emscripten as we don't want to inadvertently use
# Windows versions if we happen to be cross-compiling from a Windows platform
set(PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME;USE_UUID_GENERATE;USE_STRERROR_R;USE_ATOLL")

# TODO temporary messages - remove.
message(STATUS "COMPILE_WARNING_FLAGS: ${COMPILE_WARNING_FLAGS}")
message(STATUS "PLATFORM_DEFINITIONS: ${PLATFORM_DEFINITIONS}")
message(STATUS "UUID_LIB: ${UUID_LIB}")
message(STATUS "SSL_LIB: ${SSL_LIB}")
message(STATUS "TIME_LIB: ${TIME_LIB}")
message(STATUS "PLATFORM_LIBS: ${PLATFORM_LIBS}") # can be empty for emscripten



# The following is copied from the the main proton-c/CMakeLists.txt and annotated where there are differences
# to make it easier to refactor if we want to keep the main compilation and cross-compilation consistent.
# The main difference is prefixing paths with ${PN_PATH}/ as we can't use a relative path from this CMakeLists.txt.

set(qpid-proton-platform
  ${pn_driver_impl}
  ${PN_PATH}/src/platform.c
  ${pn_driver_ssl_impl}
  )

set(qpid-proton-core
  ${PN_PATH}/src/object/object.c

  ${PN_PATH}/src/util.c
  ${PN_PATH}/src/error.c
  ${PN_PATH}/src/buffer.c
  ${PN_PATH}/src/parser.c
  ${PN_PATH}/src/scanner.c
  ${PN_PATH}/src/types.c

  ${PN_PATH}/src/framing/framing.c

  ${PN_PATH}/src/codec/codec.c
  ${PN_PATH}/src/codec/decoder.c
  ${PN_PATH}/src/codec/encoder.c

  ${PN_PATH}/src/dispatcher/dispatcher.c
  ${PN_PATH}/src/engine/engine.c
  ${PN_PATH}/src/transport/transport.c
  ${PN_PATH}/src/message/message.c
  ${PN_PATH}/src/sasl/sasl.c

  ${PN_PATH}/src/messenger/messenger.c
  ${PN_PATH}/src/messenger/subscription.c
  ${PN_PATH}/src/messenger/store.c
  ${PN_PATH}/src/messenger/transform.c

  ${CMAKE_CURRENT_BINARY_DIR}/encodings.h
  ${CMAKE_CURRENT_BINARY_DIR}/protocol.h
  )

set_source_files_properties(
  ${qpid-proton-core}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS}"
  )

set_source_files_properties(
  ${qpid-proton-platform}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"
  COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
  )

add_library(
  qpid-proton-js SHARED

  ${qpid-proton-core}
  ${qpid-proton-platform}
  )

target_link_libraries(qpid-proton-js ${UUID_LIB} ${SSL_LIB} ${TIME_LIB} ${PLATFORM_LIBS})

set_target_properties(
  qpid-proton-js
  PROPERTIES
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED}"
  )

#add_executable(proton.js ${PN_PATH}/src/proton.c)
#target_link_libraries(proton.js qpid-proton-js)

#add_executable(proton-dump.js ${PN_PATH}/src/proton-dump.c)
#target_link_libraries(proton-dump.js qpid-proton-js)

add_executable(send-async.js ${PN_PATH}/../examples/messenger/c/send-async.c)
target_link_libraries(send-async.js qpid-proton-js)

add_executable(recv-async.js ${PN_PATH}/../examples/messenger/c/recv-async.c)
target_link_libraries(recv-async.js qpid-proton-js)

#add_executable(msgr-send.js ${PN_PATH}/../tests/tools/apps/c/msgr-common.c ${PN_PATH}/../tests/tools/apps/c/msgr-send.c)
#target_link_libraries(msgr-send.js qpid-proton-js)

#add_executable(msgr-recv.js ${PN_PATH}/../tests/tools/apps/c/msgr-common.c ${PN_PATH}/../tests/tools/apps/c/msgr-recv.c)
#target_link_libraries(msgr-recv.js qpid-proton-js)

# TODO get the uuid library and the patches in my-library.js pushed properly into emscripten ASAP
set_target_properties(
  send-async.js recv-async.js
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"
  LINK_FLAGS "-s SOCKET_DEBUG=1 --js-library ${UUID_STUB_LIB} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/my-library.js"
  )



# Some hacks so check what's getting built TODO to get rid of eventually 
#message(STATUS "qpid-proton-platform: ${qpid-proton-platform}")
#message(STATUS "qpid-proton-core: ${qpid-proton-core}")


