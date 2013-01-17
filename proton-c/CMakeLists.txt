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

include(CheckLibraryExists)
include(CheckSymbolExists)

project (Proton C)

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 3)
set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")

include(soversion.cmake)

if(WIN32 AND NOT CYGWIN)
  # linking against Windows native libraries, including mingw
  set (PN_WINAPI TRUE)
  set (PLATFORM_LIBS ws2_32 Rpcrt4)
endif(WIN32 AND NOT CYGWIN)


# In rpm builds the build sets some variables:
#  CMAKE_INSTALL_PREFIX - this is a standard cmake variable
#  INCLUDE_INSTALL_DIR
#  LIB_INSTALL_DIR
#  SYSCONF_INSTALL_DIR
#  SHARE_INSTALL_DIR
# So make these cached variables and the specific variables non cached
# and derived from them.

if (NOT DEFINED LIB_SUFFIX)
    get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
    if ("${LIB64}" STREQUAL "TRUE" AND ${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
        set(LIB_SUFFIX 64)
    else()
        set(LIB_SUFFIX "")
    endif()
endif()

set (INCLUDE_INSTALL_DIR include CACHE PATH "Include file directory")
set (LIB_INSTALL_DIR "lib${LIB_SUFFIX}" CACHE PATH "Library object file directory")
set (SYSCONF_INSTALL_DIR etc CACHE PATH "System read only configuration directory")
set (SHARE_INSTALL_DIR share CACHE PATH "Shared read only data directory")
set (MAN_INSTALL_DIR share/man CACHE PATH "Manpage directory")

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})

# Can't use ${CMAKE_VERSION) as it is not available in all versions of cmake 2.6
if ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" VERSION_LESS "2.8.0")
    # OPTIONAL does not exist in install before 2.8 so always make docs and install
    set (OPTIONAL_ARG "")
    add_custom_target(docs ALL)
    # There are bugs in the OpenSSL detection that mean -lcrypto is missed from the link line
    # so turn off unknown symbol warnings
    set (NOENABLE_UNDEFINED_ERROR ON)
else()
    set (OPTIONAL_ARG OPTIONAL)
    add_custom_target(docs)
endif()

# Set the default SSL/TLS implementation
find_package(OpenSSL)

set(ssl_impl, none)
if (OPENSSL_FOUND)
   set(ssl_impl openssl)
endif(OPENSSL_FOUND)
set(SSL_IMPL ${ssl_impl} CACHE STRING "Library to use for SSL/TLS support. Valid values: 'none','openssl'")

configure_file (
  "${PROJECT_SOURCE_DIR}/pn_config.h.in"
  "${PROJECT_BINARY_DIR}/pn_config.h"
)

include_directories ("${PROJECT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/include")

add_custom_command (
  OUTPUT ${PROJECT_BINARY_DIR}/encodings.h
  COMMAND python ${PROJECT_SOURCE_DIR}/env.py PYTHONPATH=${PROJECT_SOURCE_DIR} python ${PROJECT_SOURCE_DIR}/src/codec/encodings.h.py > ${PROJECT_BINARY_DIR}/encodings.h
  DEPENDS ${PROJECT_SOURCE_DIR}/src/codec/encodings.h.py
)

add_custom_command (
  OUTPUT ${PROJECT_BINARY_DIR}/protocol.h
  COMMAND python ${PROJECT_SOURCE_DIR}/env.py PYTHONPATH=${PROJECT_SOURCE_DIR} python ${PROJECT_SOURCE_DIR}/src/protocol.h.py > ${PROJECT_BINARY_DIR}/protocol.h
  DEPENDS ${PROJECT_SOURCE_DIR}/src/protocol.h.py
)

# Select driver
if(PN_WINAPI)
  set (pn_driver_impl src/windows/driver.c)
else(PN_WINAPI)
  set (pn_driver_impl src/driver.c)
endif(PN_WINAPI)

# Link in openssl if present
if (SSL_IMPL STREQUAL openssl)
  set (pn_driver_ssl_impl src/ssl/openssl.c)
  set (SSL_LIB ${OPENSSL_LIBRARIES})
else (SSL_IMPL STREQUAL openssl)
  set (pn_driver_ssl_impl src/ssl/ssl_stub.c)
endif (SSL_IMPL STREQUAL openssl)

# First check whether we get clock_gettime without any special library linked
CHECK_SYMBOL_EXISTS(clock_gettime "time.h" CLOCK_GETTIME_IN_LIBC)
if (CLOCK_GETTIME_IN_LIBC)
  list(APPEND PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME")
else (CLOCK_GETTIME_IN_LIBC)
  CHECK_LIBRARY_EXISTS (rt clock_gettime "" CLOCK_GETTIME_IN_RT)
  if (CLOCK_GETTIME_IN_RT)
    set (TIME_LIB rt)
    list(APPEND PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME")
  else (CLOCK_GETTIME_IN_RT)
    list(APPEND PLATFORM_DEFINITIONS "USE_GETTIMEOFDAY")
  endif (CLOCK_GETTIME_IN_RT)
endif (CLOCK_GETTIME_IN_LIBC)

CHECK_SYMBOL_EXISTS(uuid_generate "uuid/uuid.h" UUID_GENERATE_IN_LIBC)
if (UUID_GENERATE_IN_LIBC)
  list(APPEND PLATFORM_DEFINITIONS "USE_UUID_GENERATE")
else (UUID_GENERATE_IN_LIBC)
  CHECK_LIBRARY_EXISTS (uuid uuid_generate "" UUID_GENERATE_IN_UUID)
  if (UUID_GENERATE_IN_UUID)
    set (UUID_LIB uuid)
    list(APPEND PLATFORM_DEFINITIONS "USE_UUID_GENERATE")
  else (UUID_GENERATE_IN_UUID)
    CHECK_SYMBOL_EXISTS(uuid_create "uuid.h" UUID_CREATE_IN_LIBC)
    if (UUID_CREATE_IN_LIBC)
      list(APPEND PLATFORM_DEFINITIONS "USE_UUID_CREATE")
    else (UUID_CREATE_IN_LIBC)
      CHECK_SYMBOL_EXISTS(UuidToString "rpc.h" WIN_UUID)
      if (WIN_UUID)
        list(APPEND PLATFORM_DEFINITIONS "USE_WIN_UUID")
      else (WIN_UUID)
        message(FATAL_ERROR "No Uuid API found")
      endif (WIN_UUID)
    endif (UUID_CREATE_IN_LIBC)
  endif (UUID_GENERATE_IN_UUID)
endif (UUID_GENERATE_IN_LIBC)

if (PN_WINAPI)
CHECK_SYMBOL_EXISTS(strerror_s "string.h" STRERROR_S_IN_WINAPI)
if (STRERROR_S_IN_WINAPI)
  list(APPEND PLATFORM_DEFINITIONS "USE_STRERROR_S")
else (STRERROR_S_IN_WINAPI)
if (MINGW)
  message (STATUS, "NOTE: your MinGW version lacks a thread safe strerror")
  list(APPEND PLATFORM_DEFINITIONS "USE_OLD_STRERROR")
endif (MINGW)
endif (STRERROR_S_IN_WINAPI)
else (PN_WINAPI)
CHECK_SYMBOL_EXISTS(strerror_r "string.h" STRERROR_R_IN_LIBC)
if (STRERROR_R_IN_LIBC)
  list(APPEND PLATFORM_DEFINITIONS "USE_STRERROR_R")
endif (STRERROR_R_IN_LIBC)
endif (PN_WINAPI)

# Try to keep any platform specific overrides together here:

# MacOS has a bunch of differences in build tools and process and so we have to turn some things
# off if building there:
if (APPLE)
  set (NOBUILD_PHP ON)
  set (NOENABLE_WARNING_ERROR ON)
  set (NOENABLE_UNDEFINED_ERROR ON)
endif (APPLE)

# Add options here called <whatever> they will turn into "ENABLE_<whatever" and can be
# overridden on a platform specific basis above by NOENABLE_<whatever>
set (OPTIONS WARNING_ERROR UNDEFINED_ERROR)

foreach (OPTION ${OPTIONS})
  if (NOT "NOENABLE_${OPTION}")
    set ("DEFAULT_${OPTION}" ON)
  endif (NOT "NOENABLE_${OPTION}")
endforeach (OPTION)

# And add the option here too with help text
option(ENABLE_WARNING_ERROR "Consider compiler warnings to be errors" ${DEFAULT_WARNING_ERROR})
option(ENABLE_UNDEFINED_ERROR "Check for unresolved library symbols" ${DEFAULT_UNDEFINED_ERROR})

# Set any additional compiler specific flags
if (CMAKE_COMPILER_IS_GNUCC)
  if (ENABLE_WARNING_ERROR)
    set (WERROR "-Werror")
  endif (ENABLE_WARNING_ERROR)
  set (COMPILE_WARNING_FLAGS "${WERROR} -Wall -pedantic-errors")
  set (COMPILE_LANGUAGE_FLAGS "-std=c99")
  set (COMPILE_PLATFORM_FLAGS "-std=gnu99")
  if (ENABLE_UNDEFINED_ERROR)
    set (CATCH_UNDEFINED "-Wl,--no-undefined")
    set (ALLOW_UNDEFINED "-Wl,--allow-shlib-undefined")
  endif (ENABLE_UNDEFINED_ERROR)
endif (CMAKE_COMPILER_IS_GNUCC)

find_package(SWIG)
if (SWIG_FOUND)
  add_subdirectory(bindings)
endif (SWIG_FOUND)

add_subdirectory(docs/api)
add_subdirectory(docs/man)
add_subdirectory(examples/messenger/c)

set (qpid-proton-platform
  ${pn_driver_impl}
  src/platform.c
  ${pn_driver_ssl_impl}
)

set (qpid-proton-core
  src/util.c
  src/error.c
  src/buffer.c
  src/parser.c
  src/scanner.c
  src/types.c

  src/framing/framing.c

  src/codec/codec.c

  src/dispatcher/dispatcher.c
  src/engine/engine.c
  src/message/message.c
  src/sasl/sasl.c

  src/messenger.c

  ${PROJECT_BINARY_DIR}/encodings.h
  ${PROJECT_BINARY_DIR}/protocol.h
)

set_source_files_properties (
    ${qpid-proton-core}
    PROPERTIES
    COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS}"
)

set_source_files_properties (
    ${qpid-proton-platform}
    PROPERTIES
    COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"
    COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
)

add_library (
  qpid-proton SHARED

  ${qpid-proton-core}
  ${qpid-proton-platform}
)

target_link_libraries (qpid-proton ${UUID_LIB} ${SSL_LIB} ${TIME_LIB} ${PLATFORM_LIBS})

set_target_properties (
  qpid-proton
  PROPERTIES
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED}"
)

add_executable (proton src/proton.c)
target_link_libraries (proton qpid-proton)

add_executable (proton-dump src/proton-dump.c)
target_link_libraries (proton-dump qpid-proton)

set_target_properties (
    proton proton-dump
    PROPERTIES
    COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS}"
)

# Install executables and libraries
install (TARGETS proton proton-dump qpid-proton
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install header files
file(GLOB headers "include/proton/*.[hi]")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton)

# Install documentation files
install (FILES LICENSE README TODO
         DESTINATION ${PROTON_SHARE})

# Pkg config file
# Compatible variable names used in the pkg config files also for autoconf
get_filename_component (PREFIX ${CMAKE_INSTALL_PREFIX} ABSOLUTE)
get_filename_component (EXEC_PREFIX ${CMAKE_INSTALL_PREFIX} ABSOLUTE)
get_filename_component (LIBDIR ${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR} ABSOLUTE)
get_filename_component (INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/${INCLUDE_INSTALL_DIR} ABSOLUTE)

configure_file(${PROJECT_SOURCE_DIR}/src/libqpid-proton.pc.in
         ${PROJECT_BINARY_DIR}/libqpid-proton.pc @ONLY)
install (FILES ${PROJECT_BINARY_DIR}/libqpid-proton.pc
         DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
