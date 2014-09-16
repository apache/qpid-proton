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
 
# This file allows cross-compiling of proton to JavaScript using emscripten
# (https://github.com/kripken/emscripten). As it is really a cross-compilation
# (as opposed to a binding like with swig) the approach is rather different and
# somewhat replicates the main build in the proton-c/CMakeLists.txt using quite
# a bit of "copy and paste reuse".
# TODO refactor this file (and proton-c/CMakeLists.txt) to keep the main
# compilation and cross-compilation consistent.

message(STATUS "Found emscripten, using that to build JavaScript binding")

# Find and install the node.js packages that we might need. We can assume that
# node.js itself is installed because Emscripten has a dependency on it.
find_package(NodePackages)

# Describe the target OS we are building to - Emscripten mimics the Linux platform.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_CROSSCOMPILING TRUE)

# Specify the compiler to use for cross-compilation.
set(CMAKE_C_COMPILER "${EMCC}")

# Don't do compiler autodetection, since we are cross-compiling.
include(CMakeForceCompiler)
CMAKE_FORCE_C_COMPILER("${CMAKE_C_COMPILER}" Clang)

# The Proton default build type is RelWithDebInfo, but for JavaScript the C debug
# mechanism is irrelevant. If Debug is explicitly set we turn off optimisations
# and don't run the closure compiler so any error/profiling messages are readable.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "JavaScript build type is \"Debug\"")
else()
    set (CMAKE_BUILD_TYPE Release)
    message(STATUS "JavaScript build type is \"Release\"")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    set(EMSCRIPTEN_LINK_OPTIMISATIONS "-O2 --closure 1")
endif()

# From this point we should be using emscripten compilation tools.
message(STATUS "emscripten compilation environment:")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")


# Set this to the proton-c directory, we're cross-compiling code from there.
set(PN_PATH ${CMAKE_SOURCE_DIR}/proton-c)


# TODO the OpenSSL stuff won't work for emscripten by default. It might well be
# possible to compile it from source using emscripten (that's the standard practice
# for libraries with emscripten) see https://github.com/kripken/emscripten/wiki/FAQ
# "Q. How do I link against system libraries like SDL, boost, etc.?"
# Though it might be more natural to simply use a TLS protected wss:// WebSocket URL.
#  set(pn_driver_ssl_impl src/ssl/openssl.c)
#  set(SSL_LIB ${OPENSSL_LIBRARIES})
set(pn_driver_ssl_impl ${PN_PATH}/src/ssl/ssl_stub.c)

# emscripten is Linux like so use the posix driver.
set(pn_io_impl ${PN_PATH}/src/posix/io.c)
set(pn_selector_impl ${PN_PATH}/src/posix/selector.c)
set(pn_driver_impl ${PN_PATH}/src/posix/driver.c)


# Generate encodings.h and protocol.h
# It may be possible to use the ones generated for the main proton-c build but
# qpid-proton-core has a dependency on the generated files, so I'm not sure if
# it'll work without a command that can be run as a dependency. Probably best
# do it this way when cross-compiling - though more copy'n'paste
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

set(COMPILE_WARNING_FLAGS "-Werror -Wall -pedantic-errors -Wno-comment -Wno-warn-absolute-paths")

# Explicitly set PLATFORM_DEFINITIONS to Linux ones for emscripten as we don't
# want to inadvertently use Windows versions if we happen to be cross-compiling
# from a Windows platform
set(PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME;USE_UUID_GENERATE;USE_STRERROR_R;USE_ATOLL")

# The following is largely a copy from the the main proton-c/CMakeLists.txt.
# The main difference is prefixing paths with ${PN_PATH}/ as we can't use a
# relative path from this CMakeLists.txt.

set(qpid-proton-platform
  ${pn_io_impl}
  ${pn_selector_impl}
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
  ${PN_PATH}/src/engine/event.c
  ${PN_PATH}/src/transport/transport.c
  ${PN_PATH}/src/message/message.c
  ${PN_PATH}/src/sasl/sasl.c

  ${PN_PATH}/src/messenger/messenger.c
  ${PN_PATH}/src/messenger/subscription.c
  ${PN_PATH}/src/messenger/store.c
  ${PN_PATH}/src/messenger/transform.c
  ${PN_PATH}/src/selectable.c

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

# Compile Proton into LLVM bitcode which will be used later in the linking stage
# of add_executable for compilation into Javascript.
add_library(
  qpid-proton-bitcode SHARED

  ${qpid-proton-core}
  ${qpid-proton-platform}
  )

set_target_properties(
  qpid-proton-bitcode
  PROPERTIES
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED}"
  )


# Compile the send-async.c and recv-async.c examples into JavaScript
add_executable(send-async.js ${PN_PATH}/../examples/messenger/c/send-async.c)
target_link_libraries(send-async.js qpid-proton-bitcode)

add_executable(recv-async.js ${PN_PATH}/../examples/messenger/c/recv-async.c)
target_link_libraries(recv-async.js qpid-proton-bitcode)

set_target_properties(
  send-async.js recv-async.js
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"
  RUNTIME_OUTPUT_DIRECTORY examples
  DEPENDS ws

  LINK_FLAGS "-s \"WEBSOCKET_SUBPROTOCOL='AMQPWSB10'\" -${EMSCRIPTEN_LINK_OPTIMISATIONS}"
  )

# Build the main JavaScript library called proton.js
add_executable(proton.js binding.c)
target_link_libraries(proton.js qpid-proton-bitcode)

set_target_properties(
  proton.js
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"

  # The --memory-init-file 0 stops emscripten emitting a separate memory
  # initialization file, if this was enabled it makes packaging harder as
  # applications would expect proton.js.mem to be served too. It's even more
  # fiddly with node.js packages. This behaviour might be reinstated if the
  # packaging mechanism improves.

  LINK_FLAGS "-s \"EXPORT_NAME='proton'\" -s \"WEBSOCKET_SUBPROTOCOL='AMQPWSB10'\" ${EMSCRIPTEN_LINK_OPTIMISATIONS} --memory-init-file 0 --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/binding-open.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/module.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/error.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/messenger.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/subscription.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/message.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-uuid.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-symbol.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-described.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-array.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-typed-number.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-long.js --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/data-binary.js --post-js ${CMAKE_CURRENT_SOURCE_DIR}/binding-close.js -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=\"[]\" -s EXPORTED_FUNCTIONS=\"['_pn_get_version_major', '_pn_get_version_minor', '_pn_bytes', '_pn_error_text', '_pn_code', '_pn_messenger', '_pn_messenger_name', '_pn_messenger_set_blocking', '_pn_messenger_free', '_pn_messenger_errno', '_pn_messenger_error', '_pn_messenger_get_outgoing_window', '_pn_messenger_set_outgoing_window', '_pn_messenger_get_incoming_window', '_pn_messenger_set_incoming_window', '_pn_messenger_start', '_pn_messenger_stop', '_pn_messenger_stopped', '_pn_messenger_subscribe', '_pn_messenger_put', '_pn_messenger_status', '_pn_messenger_buffered', '_pn_messenger_settle', '_pn_messenger_outgoing_tracker', '_pn_messenger_work', '_pn_messenger_recv', '_pn_messenger_receiving', '_pn_messenger_get', '_pn_messenger_incoming_tracker', '_pn_messenger_incoming_subscription', '_pn_messenger_accept', '_pn_messenger_reject', '_pn_messenger_outgoing', '_pn_messenger_incoming',  '_pn_messenger_route', '_pn_messenger_rewrite', '_pn_subscription_get_context', '_pn_subscription_set_context', '_pn_subscription_address', '_pn_message', '_pn_message_id', '_pn_message_correlation_id', '_pn_message_free', '_pn_message_errno', '_pn_message_error', '_pn_message_clear', '_pn_message_is_inferred', '_pn_message_set_inferred', '_pn_message_is_durable', '_pn_message_set_durable', '_pn_message_get_priority', '_pn_message_set_priority', '_pn_message_get_ttl', '_pn_message_set_ttl', '_pn_message_is_first_acquirer', '_pn_message_set_first_acquirer', '_pn_message_get_delivery_count', '_pn_message_set_delivery_count', '_pn_message_get_user_id', '_pn_message_set_user_id', '_pn_message_get_address', '_pn_message_set_address', '_pn_message_get_subject', '_pn_message_set_subject', '_pn_message_get_reply_to', '_pn_message_set_reply_to', '_pn_message_get_content_type', '_pn_message_set_content_type', '_pn_message_get_content_encoding', '_pn_message_set_content_encoding', '_pn_message_get_expiry_time', '_pn_message_set_expiry_time', '_pn_message_get_creation_time', '_pn_message_set_creation_time', '_pn_message_get_group_id', '_pn_message_set_group_id', '_pn_message_get_group_sequence', '_pn_message_set_group_sequence', '_pn_message_get_reply_to_group_id', '_pn_message_set_reply_to_group_id', '_pn_message_encode', '_pn_message_decode', '_pn_message_instructions', '_pn_message_annotations', '_pn_message_properties', '_pn_message_body', '_pn_data', '_pn_data_free', '_pn_data_error', '_pn_data_errno', '_pn_data_clear', '_pn_data_rewind', '_pn_data_next', '_pn_data_prev', '_pn_data_enter', '_pn_data_exit', '_pn_data_lookup', '_pn_data_narrow', '_pn_data_widen', '_pn_data_type', '_pn_data_encode', '_pn_data_decode', '_pn_data_put_list', '_pn_data_put_map', '_pn_data_put_array', '_pn_data_put_described', '_pn_data_put_null', '_pn_data_put_bool', '_pn_data_put_ubyte', '_pn_data_put_byte', '_pn_data_put_ushort', '_pn_data_put_short', '_pn_data_put_uint', '_pn_data_put_int', '_pn_data_put_char', '_pn_data_put_ulong', '_pn_data_put_long', '_pn_data_put_timestamp', '_pn_data_put_float', '_pn_data_put_double', '_pn_data_put_decimal32', '_pn_data_put_decimal64', '_pn_data_put_decimal128', '_pn_data_put_uuid', '_pn_data_put_binary', '_pn_data_put_string', '_pn_data_put_symbol', '_pn_data_get_list', '_pn_data_get_map', '_pn_data_get_array', '_pn_data_is_array_described', '_pn_data_get_array_type', '_pn_data_is_described', '_pn_data_is_null', '_pn_data_get_bool', '_pn_data_get_ubyte', '_pn_data_get_byte', '_pn_data_get_ushort', '_pn_data_get_short', '_pn_data_get_uint', '_pn_data_get_int', '_pn_data_get_char', '_pn_data_get_ulong', '_pn_data_get_long', '_pn_data_get_timestamp', '_pn_data_get_float', '_pn_data_get_double', '_pn_data_get_decimal32', '_pn_data_get_decimal64', '_pn_data_get_decimal128', '_pn_data_get_uuid', '_pn_data_get_binary', '_pn_data_get_string', '_pn_data_get_symbol', '_pn_data_copy', '_pn_data_format', '_pn_data_dump']\""
  )

# This command packages up the compiled proton.js into a node.js package called
# qpid-proton and copies it to the <proton>/node_modules directory. This allows
# the node.js test and example programs to do proton = require("qpid-proton");
add_custom_command(
    TARGET proton.js
    COMMAND ${CMAKE_COMMAND}
            -E copy_directory 
            ${CMAKE_CURRENT_SOURCE_DIR}/qpid-proton 
            ${PROJECT_SOURCE_DIR}/node_modules/qpid-proton
    COMMAND ${CMAKE_COMMAND}
            -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/proton.js
            ${PROJECT_SOURCE_DIR}/node_modules/qpid-proton/lib
   COMMENT "Building qpid-proton package for node.js"
)

# The cleanall target removes the qpid-proton package from <proton>/node_modules
add_custom_target(
    cleanall
    COMMAND echo "CLEAN NODE MODULES"
    COMMAND ${CMAKE_COMMAND}
            -E remove_directory
            ${PROJECT_SOURCE_DIR}/node_modules/qpid-proton
)

# If the docs target is specified and the jsdoc3 package for node.js has been
# installed then build the JavaScript API documentation.
if (NODE_JSDOC_FOUND)
    set(JSDOC_EXE ${PROJECT_SOURCE_DIR}/node_modules/.bin/jsdoc)

    message(STATUS "Documentation Enabled. Using ${JSDOC_EXE} to build JavaScript docs")
    add_custom_target(docs-js COMMAND ${JSDOC_EXE}
                      -d ${CMAKE_CURRENT_BINARY_DIR}/html
                      ${CMAKE_CURRENT_SOURCE_DIR}/module.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/error.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/messenger.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/subscription.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/message.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-uuid.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-symbol.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-described.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-array.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-typed-number.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-long.js
                      ${CMAKE_CURRENT_SOURCE_DIR}/data-binary.js)
    add_dependencies(docs docs-js)
endif (NODE_JSDOC_FOUND)

