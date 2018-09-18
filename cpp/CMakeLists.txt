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
cmake_minimum_required(VERSION 2.8.12)

enable_language(CXX)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)

include(versions.cmake)

set (BUILD_CPP_03 OFF CACHE BOOL "Compile the C++ binding as C++03 even when C++11 is available")

# Check for JSON-CPP support for connection configuration
find_package(JsonCpp)
option(ENABLE_JSONCPP "Use jsoncpp parser for connection configuration" ${JsonCpp_FOUND})
if (ENABLE_JSONCPP)
  set(CONNECT_CONFIG_SRC src/connect_config.cpp)
  set(CONNECT_CONFIG_LIBS ${JsonCpp_LIBRARY})
else()
  set(CONNECT_CONFIG_SRC src/connect_config_dummy.cpp)
endif()

# This effectively checks for cmake version 3.1 or later
if (DEFINED CMAKE_CXX_COMPILE_FEATURES)
  if (BUILD_CPP_03)
    set(STD 98)
  else ()
    set(STD 11)
    list(APPEND PLATFORM_LIBS Threads::Threads)
  endif ()
  set(CMAKE_CXX_STANDARD ${STD})
  set(CMAKE_CXX_EXTENSIONS OFF)
  if (MSVC)  # Compiler feature checks only needed for Visual Studio in this case
    include(cpp.cmake)
  elseif (STD EQUAL 11)
    set(CPP_DEFINITIONS "HAS_CPP11")
  endif()
else ()
  if (BUILD_CPP_03)
    set(CXX_STANDARD "-std=c++98")
  else ()
    include(CheckCXXCompilerFlag)
    # These flags work with GCC/Clang/SunPro compilers
    check_cxx_compiler_flag("-std=c++11" ACCEPTS_CXX11)
    check_cxx_compiler_flag("-std=c++0x" ACCEPTS_CXX0X)
    if (ACCEPTS_CXX11)
      set(CXX_STANDARD "-std=c++11")
      set(CPP_DEFINITIONS "HAS_CPP11")
      list(APPEND PLATFORM_LIBS Threads::Threads)
    elseif(ACCEPTS_CXX0X)
      set(CXX_STANDARD "-std=c++0x")
      list(APPEND PLATFORM_LIBS Threads::Threads)
      include(cpp.cmake) # Compiler checks needed for C++0x as not all C++11 may be supported
    else()
      include(cpp.cmake) # Compiler checks needed as we have no idea whats going on here!
    endif()
  endif()
endif ()

# Construct #define lines to insert in config_presets.hpp
foreach(d ${CPP_DEFINITIONS})
  set(presets "${presets}#define PN_CPP_LIB_${d} 1\n")
endforeach()
# For Visual Studio define the app compile time macros now, for everything else don't
if(MSVC)
  set(presets "${presets}\n// Compiled for MSVC version ${CMAKE_CXX_COMPILER_VERSION} (${MSVC_VERSION})\n")
  foreach(d ${CPP_DEFINITIONS})
    set(presets "${presets}# define PN_CPP_${d} 1\n")
  endforeach()
else()
  set(presets "${presets}\n#if qpid_proton_cpp_EXPORTS\n")
  foreach(d ${CPP_DEFINITIONS})
    set(presets "${presets}# define PN_CPP_${d} 1\n")
  endforeach()
  set(presets "${presets}#endif // qpid_proton_cpp_EXPORTS\n")
endif()

configure_file(config_presets.hpp.in config_presets.hpp @ONLY)

set(CXX_EXAMPLE_FLAGS "${CXX_WARNING_FLAGS} ${CXX_STANDARD}")
set(CXX_EXAMPLE_LINK_FLAGS "${SANITIZE_FLAGS}")

include_directories (
  "${CMAKE_SOURCE_DIR}/c/include"
  "${CMAKE_SOURCE_DIR}/c/src" # Here because of a naughty looking dependency on message-internal.h
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${PN_C_INCLUDE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  )

add_definitions(${CXX_STANDARD} ${CXX_WARNING_FLAGS} "-DPN_CPP_USE_DEPRECATED_API=1")

set(qpid-proton-cpp-source
  src/binary.cpp
  src/byte_array.cpp
  src/map.cpp
  src/connection.cpp
  src/connection_driver.cpp
  src/connection_options.cpp
  src/container.cpp
  src/proactor_container_impl.cpp
  src/contexts.cpp
  src/data.cpp
  src/decimal.cpp
  src/decoder.cpp
  src/delivery.cpp
  src/duration.cpp
  src/encoder.cpp
  src/endpoint.cpp
  src/error.cpp
  src/error_condition.cpp
  src/handler.cpp
  src/link.cpp
  src/link_namer.cpp
  src/listener.cpp
  src/message.cpp
  src/messaging_adapter.cpp
  src/node_options.cpp
  src/null.cpp
  src/object.cpp
  src/proton_bits.cpp
  src/receiver.cpp
  src/receiver_options.cpp
  src/reconnect_options.cpp
  src/returned.cpp
  src/sasl.cpp
  src/scalar_base.cpp
  src/sender.cpp
  src/sender_options.cpp
  src/session.cpp
  src/session_options.cpp
  src/source.cpp
  src/ssl.cpp
  src/ssl_domain.cpp
  src/target.cpp
  src/terminus.cpp
  src/timestamp.cpp
  src/tracker.cpp
  src/transfer.cpp
  src/transport.cpp
  src/type_id.cpp
  src/url.cpp
  src/uuid.cpp
  src/value.cpp
  src/work_queue.cpp
  ${CONNECT_CONFIG_SRC}
  )

set_source_files_properties (
  ${qpid-proton-cpp-source}
  PROPERTIES
  COMPILE_FLAGS "${LTO}"
  )

if (MSVC)
  set (CMAKE_DEBUG_POSTFIX "d")
endif ()

add_library(qpid-proton-cpp SHARED ${qpid-proton-cpp-source})
if(BUILD_STATIC_LIBS)
  add_library(qpid-proton-cpp-static STATIC ${qpid-proton-cpp-source})
endif(BUILD_STATIC_LIBS)

target_link_libraries (qpid-proton-cpp LINK_PRIVATE ${PLATFORM_LIBS} qpid-proton-core qpid-proton-proactor ${CONNECT_CONFIG_LIBS})

set_target_properties (
  qpid-proton-cpp
  PROPERTIES
  LINKER_LANGUAGE CXX
  VERSION   "${PN_LIB_CPP_VERSION}"
  SOVERSION "${PN_LIB_CPP_MAJOR_VERSION}"
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

install (DIRECTORY "include/proton" DESTINATION ${INCLUDE_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/config_presets.hpp" DESTINATION "${INCLUDE_INSTALL_DIR}/proton/internal")
install (DIRECTORY "examples/"
  DESTINATION "${PROTON_SHARE}/examples/cpp"
  USE_SOURCE_PERMISSIONS
  PATTERN "ProtonCppConfig.cmake" EXCLUDE)

add_subdirectory(examples)
add_subdirectory(docs)

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

macro(add_cpp_test test)
  add_executable (${test} src/${test}.cpp)
  target_link_libraries (${test} qpid-proton-cpp ${PLATFORM_LIBS})
  if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_test (NAME cpp-${test}
      COMMAND ${PN_ENV_SCRIPT}
      "PATH=$<TARGET_FILE_DIR:qpid-proton>"
      $<TARGET_FILE:${test}> ${ARGN})
  else ()
    add_test (NAME cpp-${test} COMMAND ${TEST_EXE_PREFIX_CMD} $<TARGET_FILE:${test}> ${ARGN})
  endif ()
endmacro(add_cpp_test)

add_cpp_test(codec_test)
add_cpp_test(connection_driver_test)
add_cpp_test(interop_test ${CMAKE_SOURCE_DIR}/tests)
add_cpp_test(message_test)
add_cpp_test(map_test)
add_cpp_test(scalar_test)
add_cpp_test(value_test)
add_cpp_test(container_test)
add_cpp_test(url_test)
add_cpp_test(reconnect_test)
add_cpp_test(link_test)
if (ENABLE_JSONCPP)
  # Directories needed by connect_config tests
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/testdata/.config/messaging")
  add_cpp_test(connect_config_test)
  set_tests_properties(cpp-connect_config_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()
