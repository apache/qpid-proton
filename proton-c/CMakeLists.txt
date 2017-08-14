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

include(CheckLibraryExists)
include(CheckSymbolExists)

include(soversion.cmake)

add_custom_target(docs)
add_custom_target(doc DEPENDS docs)

# Set the default SSL/TLS implementation
find_package(OpenSSL)
find_package(PythonInterp REQUIRED)
find_package(SWIG)
# FindSwig.cmake "forgets" make its outputs advanced like a good citizen
mark_as_advanced(SWIG_DIR SWIG_EXECUTABLE SWIG_VERSION)

# See if Cyrus SASL is available
find_library(CYRUS_SASL_LIBRARY sasl2)
find_path(CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args(CyrusSASL DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
mark_as_advanced(CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)

# Find saslpasswd2 executable to generate test config
find_program(SASLPASSWD_EXE saslpasswd2 DOC "Program used to make SASL user db for testing")
mark_as_advanced(SASLPASSWD_EXE)

if(WIN32 AND NOT CYGWIN)
  # linking against Windows native libraries, including mingw
  set (PN_WINAPI TRUE)
endif(WIN32 AND NOT CYGWIN)

set(ssl_impl, none)
if(PN_WINAPI)
  set(ssl_impl schannel)
  set(ssl_providers "'none','schannel','openssl'")
else(PN_WINAPI)
  if (OPENSSL_FOUND)
    set(ssl_impl openssl)
  endif (OPENSSL_FOUND)
  set(ssl_providers "'none','openssl'")
endif(PN_WINAPI)
set(SSL_IMPL ${ssl_impl} CACHE STRING "Library to use for SSL/TLS support. Valid values: ${ssl_providers}")

set(sasl_providers cyrus none)
if (CYRUSSASL_FOUND)
  set (sasl_impl cyrus)
else ()
  set (sasl_impl none)
endif ()
set(SASL_IMPL ${sasl_impl} CACHE STRING "Library to use for SASL support. Valid values: ${sasl_providers}")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/include/proton/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/proton/version.h"
  )

include_directories ("${CMAKE_CURRENT_BINARY_DIR}/src")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}/include")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/include")

set (env_py ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/env.py)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  COMMAND ${env_py} PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR} ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/encodings.h.py > ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/encodings.h.py
  )

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  COMMAND ${env_py} PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR} ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol.h.py > ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol.h.py
  )

add_custom_target(
  generated_c_files
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  )

# Select IO impl
if(PN_WINAPI)
  set (pn_io_impl src/reactor/io/windows/io.c src/reactor/io/windows/iocp.c src/reactor/io/windows/write_pipeline.c)
  set (pn_selector_impl src/reactor/io/windows/selector.c)
else(PN_WINAPI)
  set (pn_io_impl src/reactor/io/posix/io.c)
  set (pn_selector_impl src/reactor/io/posix/selector.c)
endif(PN_WINAPI)

# Link in SASL if present
if (SASL_IMPL STREQUAL cyrus)
  set(pn_sasl_impl src/sasl/sasl.c src/sasl/default_sasl.c src/sasl/cyrus_sasl.c)
  include_directories (${CYRUS_SASL_INCLUDE_DIR})
  set(SASL_LIB ${CYRUS_SASL_LIBRARY} -lpthread)
elseif (SASL_IMPL STREQUAL none)
  set(pn_sasl_impl src/sasl/sasl.c src/sasl/default_sasl.c src/sasl/cyrus_stub.c)
endif ()

# Set Compiler extra flags for Solaris when using SunStudio
if(CMAKE_CXX_COMPILER_ID STREQUAL "SunPro" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mt" )
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "SunPro" )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mt")
endif()

# Link in openssl if present
if (SSL_IMPL STREQUAL openssl)
  set (pn_ssl_impl src/ssl/openssl.c)
  include_directories (${OPENSSL_INCLUDE_DIR})
  set (SSL_LIB ${OPENSSL_LIBRARIES})
elseif (SSL_IMPL STREQUAL schannel)
  set (pn_ssl_impl src/ssl/schannel.c)
  set (SSL_LIB Crypt32.lib Secur32.lib)
else ()
  set (pn_ssl_impl src/ssl/ssl_stub.c)
endif ()

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
    CHECK_SYMBOL_EXISTS(GetSystemTimeAsFileTime "windows.h" WINDOWS_FILETIME)
    if (WINDOWS_FILETIME)
      list(APPEND PLATFORM_DEFINITIONS "USE_WIN_FILETIME")
    else (WINDOWS_FILETIME)
      list(APPEND PLATFORM_DEFINITIONS "USE_GETTIMEOFDAY")
    endif (WINDOWS_FILETIME)
  endif (CLOCK_GETTIME_IN_RT)
endif (CLOCK_GETTIME_IN_LIBC)

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

CHECK_SYMBOL_EXISTS(atoll "stdlib.h" C99_ATOLL)
if (C99_ATOLL)
  list(APPEND PLATFORM_DEFINITIONS "USE_ATOLL")
else (C99_ATOLL)
  CHECK_SYMBOL_EXISTS(_atoi64 "stdlib.h" WINAPI_ATOI64)
  if (WINAPI_ATOI64)
    list(APPEND PLATFORM_DEFINITIONS "USE_ATOI64")
  else (WINAPI_ATOI64)
    message(FATAL_ERROR "No atoll API found")
  endif (WINAPI_ATOI64)
endif (C99_ATOLL)

if (PN_WINAPI)
  set (PLATFORM_LIBS ws2_32 Rpcrt4)
  list(APPEND PLATFORM_DEFINITIONS "PN_WINAPI")
endif (PN_WINAPI)

# Try to keep any platform specific overrides together here:

# Until we can decide what to do with PHP support, turn it off by default
# (We can't build with recent versions of PHP)
set (NOBUILD_PHP ON)

# MacOS has a bunch of differences in build tools and process and so we have to turn some things
# off if building there:
if (APPLE)
  set (NOBUILD_PHP ON)
  set (NOENABLE_WARNING_ERROR ON)
  set (NOENABLE_UNDEFINED_ERROR ON)
endif (APPLE)

# Make LTO default to off until we can figure out the valgrind issues
set (NOENABLE_LINKTIME_OPTIMIZATION ON)

# Add options here called <whatever> they will turn into "ENABLE_<whatever" and can be
# overridden on a platform specific basis above by NOENABLE_<whatever>
set (OPTIONS WARNING_ERROR UNDEFINED_ERROR LINKTIME_OPTIMIZATION HIDE_UNEXPORTED_SYMBOLS)

foreach (OPTION ${OPTIONS})
  if (NOT NOENABLE_${OPTION})
    set ("DEFAULT_${OPTION}" ON)
  endif ()
endforeach (OPTION)

# And add the option here too with help text
option(ENABLE_WARNING_ERROR "Consider compiler warnings to be errors" ${DEFAULT_WARNING_ERROR})
option(ENABLE_UNDEFINED_ERROR "Check for unresolved library symbols" ${DEFAULT_UNDEFINED_ERROR})
option(ENABLE_LINKTIME_OPTIMIZATION "Perform link time optimization" ${DEFAULT_LINKTIME_OPTIMIZATION})
option(ENABLE_HIDE_UNEXPORTED_SYMBOLS "Only export library symbols that are explicitly requested" ${DEFAULT_HIDE_UNEXPORTED_SYMBOLS})

# Set any additional compiler specific flags
if (CMAKE_COMPILER_IS_GNUCC)
  if (ENABLE_WARNING_ERROR)
    set (WERROR "-Werror")
  endif (ENABLE_WARNING_ERROR)
  set (COMPILE_WARNING_FLAGS "${WERROR} -Wall -pedantic-errors")
  # C++ allow "%z" format specifier and variadic macros
  set (CXX_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wno-format -Wno-variadic-macros")
  if (NOT BUILD_WITH_CXX)
    set (COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wstrict-prototypes")
    set (COMPILE_LANGUAGE_FLAGS "-std=c99")
    set (COMPILE_PLATFORM_FLAGS "-std=gnu99")

    execute_process(COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1} ${CMAKE_C_COMPILER_ARG2} -dumpversion OUTPUT_VARIABLE GCC_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (${GCC_VERSION} VERSION_LESS "4.3.0")
      # Only a concern if contibuting code back.
      message (STATUS "Old gcc version detected.  C++ compatibility checks disabled")
    else (${GCC_VERSION} VERSION_LESS "4.3.0")
      set (COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wc++-compat -Wvla -Wsign-compare -Wwrite-strings")
    endif (${GCC_VERSION} VERSION_LESS "4.3.0")
  else (NOT BUILD_WITH_CXX)
    set (COMPILE_WARNING_FLAGS "${CXX_WARNING_FLAGS}")
  endif (NOT BUILD_WITH_CXX)

  if (ENABLE_UNDEFINED_ERROR)
    set (CATCH_UNDEFINED "-Wl,--no-undefined")
    set (ALLOW_UNDEFINED "-Wl,--allow-shlib-undefined")
  endif (ENABLE_UNDEFINED_ERROR)

  if (ENABLE_LINKTIME_OPTIMIZATION)
    set (LTO "-flto")
  endif (ENABLE_LINKTIME_OPTIMIZATION)

  if (ENABLE_HIDE_UNEXPORTED_SYMBOLS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
  endif (ENABLE_HIDE_UNEXPORTED_SYMBOLS)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "SunPro")
  if (ENABLE_HIDE_UNEXPORTED_SYMBOLS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xldscope=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -xldscope=hidden")
  endif (ENABLE_HIDE_UNEXPORTED_SYMBOLS)
endif (CMAKE_COMPILER_IS_GNUCC)

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
  set (COMPILE_WARNING_FLAGS  "-Wall -pedantic")
  set (COMPILE_LANGUAGE_FLAGS "-std=c99")
  if (ENABLE_WARNING_ERROR)
    set (COMPILE_WARNING_FLAGS "-Werror ${COMPILE_WARNING_FLAGS}")
  endif (ENABLE_WARNING_ERROR)
  # TODO aconway 2016-01-06: we should be able to clean up the code and turn on
  # some of these warnings.
  set (COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wno-gnu-statement-expression")
  set (CXX_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal -Wno-padded -Wno-sign-conversion -Wno-switch-enum -Wno-weak-vtables -Wno-exit-time-destructors -Wno-global-constructors -Wno-shorten-64-to-32 -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-missing-noreturn")
endif()

# Make flags visible to examples.
set(CXX_WARNING_FLAGS "${CXX_WARNING_FLAGS}" PARENT_SCOPE)
set(COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS}" PARENT_SCOPE)
set(COMPILE_LANGUAGE_FLAGS "${COMPILE_LANGUAGE_FLAGS}" PARENT_SCOPE)
set(LINK_TIME_OPTIMIZATION "${LTO}" PARENT_SCOPE)

if (MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(
        /wd4244
        /wd4267
        /wd4800
        /wd4996
    )
    set (qpid-proton-platform src/compiler/msvc/snprintf.c)
endif (MSVC)

macro (pn_absolute_install_dir NAME VALUE PREFIX)
  if(IS_ABSOLUTE ${VALUE})
    set(${NAME} "${VALUE}")
  elseif(IS_ABSOLUTE ${PREFIX})
    set(${NAME} "${PREFIX}/${VALUE}")
  else()
    set(${NAME} "${CMAKE_BINARY_DIR}/${PREFIX}/${VALUE}")
  endif(IS_ABSOLUTE ${VALUE})
  get_filename_component(${NAME} ${${NAME}} ABSOLUTE)
endmacro()

pn_absolute_install_dir(PREFIX "." ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir(EXEC_PREFIX "." ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir(LIBDIR ${LIB_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir(INCLUDEDIR ${INCLUDE_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})

add_subdirectory(docs/api)
add_subdirectory(../tests/tools/apps/c ../tests/tools/apps/c)

# for full source distribution:
set (qpid-proton-platform-all
  src/platform/platform.c
  src/reactor/io/windows/io.c
  src/reactor/io/windows/iocp.c
  src/reactor/io/windows/write_pipeline.c
  src/reactor/io/windows/selector.c
  src/reactor/io/posix/io.c
  src/reactor/io/posix/selector.c
  )

# platform specific library build:
set (qpid-proton-platform-io
  src/platform/platform.c
  ${pn_io_impl}
  ${pn_selector_impl}
  )

# for full source distribution:
set (qpid-proton-layers-all
  src/sasl/sasl.c
  src/sasl/default_sasl.c
  src/sasl/cyrus_sasl.c
  src/sasl/cyrus_stub.c
  src/ssl/openssl.c
  src/ssl/schannel.c
  src/ssl/ssl_stub.c
  )

# for current build system's environment:
set (qpid-proton-layers
  ${pn_sasl_impl}
  ${pn_ssl_impl}
  )

set (qpid-proton-core
  src/core/object/object.c
  src/core/object/list.c
  src/core/object/map.c
  src/core/object/string.c
  src/core/object/iterator.c
  src/core/object/record.c

  src/core/log.c
  src/core/util.c
  src/core/error.c
  src/core/buffer.c
  src/core/types.c

  src/core/framing.c

  src/core/codec.c
  src/core/decoder.c
  src/core/encoder.c

  src/core/dispatcher.c
  src/core/connection_driver.c
  src/core/engine.c
  src/core/event.c
  src/core/autodetect.c
  src/core/transport.c
  src/core/message.c
  src/core/url-internal.c
)

set (qpid-proton-include-generated
  ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proton/version.h
  )

set (qpid-proton-private-includes
  src/extra/scanner.h
  src/messenger/store.h
  src/messenger/subscription.h
  src/messenger/messenger.h
  src/messenger/transform.h
  src/ssl/ssl-internal.h
  src/sasl/sasl-internal.h
  src/core/autodetect.h
  src/core/log_private.h
  src/core/config.h
  src/core/encoder.h
  src/core/dispatch_actions.h
  src/core/engine-internal.h
  src/core/transport.h
  src/core/framing.h
  src/core/buffer.h
  src/core/util.h
  src/core/dispatcher.h
  src/core/data.h
  src/core/decoder.h
  src/core/max_align.h
  src/core/url-internal.h
  src/reactor/io/windows/iocp.h
  src/reactor/selector.h
  src/reactor/io.h
  src/reactor/reactor.h
  src/reactor/selectable.h
  src/platform/platform.h
  src/platform/platform_fmt.h
  src/proactor/proactor-internal.h
  )

set (qpid-proton-extra
  src/extra/parser.c
  src/extra/scanner.c
  src/extra/url.c

  src/reactor/reactor.c
  src/reactor/handler.c
  src/reactor/connection.c
  src/reactor/acceptor.c
  src/reactor/selectable.c
  src/reactor/timer.c

  src/handlers/handshaker.c
  src/handlers/iohandler.c
  src/handlers/flowcontroller.c

  src/messenger/messenger.c
  src/messenger/subscription.c
  src/messenger/store.c
  src/messenger/transform.c
  )

set (qpid-proton-include
  include/proton/cid.h
  include/proton/codec.h
  include/proton/condition.h
  include/proton/connection.h
  include/proton/connection_driver.h
  include/proton/delivery.h
  include/proton/disposition.h
  include/proton/engine.h
  include/proton/error.h
  include/proton/event.h
  include/proton/import_export.h
  include/proton/link.h
  include/proton/listener.h
  include/proton/log.h
  include/proton/message.h
  include/proton/netaddr.h
  include/proton/object.h
  include/proton/proactor.h
  include/proton/sasl.h
  include/proton/sasl-plugin.h
  include/proton/session.h
  include/proton/ssl.h
  include/proton/terminus.h
  include/proton/transport.h
  include/proton/type_compat.h
  include/proton/types.h
)

set (qpid-proton-include-extra
  include/proton/handlers.h
  include/proton/messenger.h
  include/proton/parser.h
  include/proton/reactor.h
  include/proton/selectable.h
  include/proton/url.h
)

#
# Choose a proactor: user can set PROACTOR, or if not set pick a default.
# The default is the first one that passes its build test, in order listed below.
# "none" disables the proactor even if a default is available.
#
set(PROACTOR "" CACHE STRING "Override default proactor, one of: epoll, libuv, iocp, none")
string(TOLOWER "${PROACTOR}" PROACTOR)

if (PROACTOR STREQUAL "epoll" OR (NOT PROACTOR AND NOT BUILD_PROACTOR))
  check_symbol_exists(epoll_wait "sys/epoll.h" HAVE_EPOLL)
  if (HAVE_EPOLL)
    set (PROACTOR_OK epoll)
    set (qpid-proton-proactor src/proactor/epoll.c src/proactor/proactor-internal.c)
    set (PROACTOR_LIBS -lpthread)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
      )
  endif()
endif()

if (PROACTOR STREQUAL "iocp" OR (NOT PROACTOR AND NOT PROACTOR_OK))
  if(WIN32 AND NOT CYGWIN)
    message(WARNING "Windows IOCP proactor will be built as a prototype but does not yet pass tests")
    set (PROACTOR_OK iocp)
    set (qpid-proton-proactor src/proactor/win_iocp.c src/proactor/proactor-internal.c)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS} ${LTO}"
      COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
      )
  endif(WIN32 AND NOT CYGWIN)
endif()

if (PROACTOR STREQUAL "libuv" OR (NOT PROACTOR AND NOT PROACTOR_OK))
  find_package(Libuv)
  if (LIBUV_FOUND)
    set (PROACTOR_OK libuv)
    set (qpid-proton-proactor src/proactor/libuv.c src/proactor/proactor-internal.c)
    set (PROACTOR_LIBS ${Libuv_LIBRARIES})
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
      )
    include_directories(${Libuv_INCLUDE_DIRS})
  endif()
endif()

if (PROACTOR_OK)
  message(STATUS "Building the ${PROACTOR_OK} proactor")
elseif (PROACTOR AND NOT PROACTOR STREQUAL "none")
  message(FATAL_ERROR "Cannot build the ${PROACTOR} proactor")
endif()

# note: process bindings after the source lists have been defined so
# the bindings can reference them
add_subdirectory(bindings)

source_group("API Header Files" FILES ${qpid-proton-include} ${qpid-proton-include-extra})

set_source_files_properties (
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-extra}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
  )

set_source_files_properties (
  ${qpid-proton-platform}
  ${qpid-proton-platform-io}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS} ${LTO}"
  COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
  )

if (BUILD_WITH_CXX)
  set_source_files_properties (
    ${qpid-proton-core}
    ${qpid-proton-proactor}
    ${qpid-proton-layers}
    ${qpid-proton-extra}
    ${qpid-proton-platform}
    ${qpid-proton-platform-io}
    PROPERTIES LANGUAGE CXX
    )
endif (BUILD_WITH_CXX)

add_library (
  qpid-proton-core SHARED
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-platform}
  ${qpid-proton-include}
  ${qpid-proton-include-generated}
  )
add_dependencies(qpid-proton-core generated_c_files)

target_link_libraries (qpid-proton-core ${UUID_LIB} ${SSL_LIB} ${SASL_LIB} ${TIME_LIB} ${PLATFORM_LIBS})

set_target_properties (
  qpid-proton-core
  PROPERTIES
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
  )

add_library(
  qpid-proton SHARED
  # Proton Core
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-platform}
  ${qpid-proton-include}
  ${qpid-proton-include-generated}
  # Proactor
  ${qpid-proton-proactor}
  # Proton Reactor/Messenger
  ${qpid-proton-extra}
  ${qpid-proton-platform-io}
  ${qpid-proton-include-extra}
  )
add_dependencies(qpid-proton generated_c_files)

if (MSVC)
  # Add a phony dependency for Windows builds to serialize creation
  # of generated files. See issue PROTON-1376.
  # When a Windows build creates src/encodings.h and src/protocol.h
  # only once then this can be removed.
  add_dependencies(qpid-proton qpid-proton-core)
endif (MSVC)

target_link_libraries (qpid-proton LINK_PRIVATE ${UUID_LIB} ${SSL_LIB} ${SASL_LIB} ${TIME_LIB} ${PLATFORM_LIBS} ${PROACTOR_LIBS})

set_target_properties (
  qpid-proton
  PROPERTIES
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
  )

if (MSVC)
  # guard against use of C99 violating functions on Windows
  include(WindowsC99CheckDef)
endif(MSVC)

if (qpid-proton-proactor)
  # Bizarre CMake variable setting
  set(HAS_PROACTOR True)
  set(HAS_PROACTOR ${HAS_PROACTOR} PARENT_SCOPE)
  add_library (
    qpid-proton-proactor SHARED ${qpid-proton-proactor})
  target_link_libraries (qpid-proton-proactor  LINK_PUBLIC qpid-proton-core)
  target_link_libraries (qpid-proton-proactor  LINK_PRIVATE ${PLATFORM_LIBS} ${PROACTOR_LIBS})
  list(APPEND LIB_TARGETS qpid-proton-proactor)
  set_target_properties (
    qpid-proton-proactor
    PROPERTIES
    VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
    SOVERSION "${PN_LIB_SOMAJOR}"
    LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
    )
endif()

# Install executables and libraries
install(TARGETS qpid-proton qpid-proton-core ${LIB_TARGETS}
  EXPORT  proton
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install windows qpid-proton pdb files
if (MSVC)
  install(FILES $<TARGET_PDB_FILE:qpid-proton>
    DESTINATION bin
    CONFIGURATIONS RelWithDebInfo Debug
    OPTIONAL)
endif (MSVC)

# Install header files
file(GLOB headers "include/proton/*.[hi]")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton)
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/include/proton/version.h
         DESTINATION ${INCLUDE_INSTALL_DIR}/proton)

# Set ${VAR}/${VAR}DEBUG variables, configure and install the packageconf files for LIB
macro(configure_lib VAR LIB)
  if(DEFINED CMAKE_IMPORT_LIBRARY_PREFIX)
    set(LIB_PREFIX ${CMAKE_IMPORT_LIBRARY_PREFIX})
    set(LIB_SUFFIX ${CMAKE_IMPORT_LIBRARY_SUFFIX})
  else()
    set(LIB_PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX})
    set(LIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()
  set(${VAR} ${LIB_PREFIX}${LIB}${LIB_SUFFIX})
  set("${VAR}DEBUG" ${LIB_PREFIX}${LIB}${CMAKE_DEBUG_POSTFIX}${LIB_SUFFIX})
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib${LIB}.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/lib${LIB}.pc @ONLY)
  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/lib${LIB}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
endmacro()

configure_lib(PROTONLIB qpid-proton)
configure_lib(PROTONCORELIB qpid-proton-core)
if(HAS_PROACTOR)
  configure_lib(PROTONPROACTORLIB qpid-proton-proactor)
endif(HAS_PROACTOR)

include(WriteBasicConfigVersionFile)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ProtonConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfig.cmake @ONLY)
write_basic_config_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfigVersion.cmake
  VERSION ${PN_VERSION}
  COMPATIBILITY AnyNewerVersion)
install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfigVersion.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/Proton)

# c tests:

add_subdirectory(src/tests)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # No change needed for windows already use correct separator
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    set (${result} ${path} PARENT_SCOPE)
  endfunction()
else (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # Just change ';'->':'
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    string (REGEX REPLACE ";" ":" path "${path}")
    set (${result} ${path} PARENT_SCOPE)
  endfunction()
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

# python test: tests/python/proton-test
if (BUILD_PYTHON)
  set (py_root "${pn_test_root}/python")
  set (py_src "${CMAKE_CURRENT_SOURCE_DIR}/bindings/python")
  set (py_bin "${CMAKE_CURRENT_BINARY_DIR}/bindings/python")
  set (py_dll "$<TARGET_FILE_DIR:_cproton>")
  set (py_bld "$<TARGET_FILE_DIR:qpid-proton>") # For windows
  set (app_path $<TARGET_FILE_DIR:msgr-send> "${pn_test_root}/tools/apps/python")
  set (py_path ${py_bld} ${app_path} $ENV{PATH})
  set (py_pythonpath ${py_root} ${py_src} ${py_bin} ${py_dll} $ENV{PYTHONPATH})
  to_native_path ("${py_pythonpath}" py_pythonpath)
  to_native_path ("${py_path}" py_path)

  add_test (NAME python-test
            COMMAND ${env_py}
              "PATH=${py_path}" "PYTHONPATH=${py_pythonpath}"
              "CLASSPATH=${CMAKE_BINARY_DIR}/proton-j/proton-j.jar"
              "SASLPASSWD=${SASLPASSWD_EXE}"
              ${VALGRIND_ENV}
              ${PYTHON_EXECUTABLE} "${py_root}/proton-test")
  set_tests_properties(python-test PROPERTIES PASS_REGULAR_EXPRESSION "Totals: .* 0 failed")

  # Eventually, we'll get rid of this check when other
  # platforms will be supported. Since `setup.py` will skip
  # the build for non linux plaforms, it doesn't make sense
  # to try to run them.
  option(TOX_TEST "Enable muti-version python testing with TOX" ON)
  set(tox_default "py26,py27,py33,py34,py35")
  set(TOX_ENVLIST ${tox_default} CACHE STRING "List of python environments for TOX tests" )
  if (NOT (TOX_ENVLIST STREQUAL tox_default))
    message(WARNING "non-default TOX test set '${TOX_ENVLIST}' (default '${tox_default}')")
  endif()
  if (CMAKE_SYSTEM_NAME STREQUAL Linux AND TOX_TEST)
    find_program(TOX_EXE "tox")
    if (CMAKE_BUILD_TYPE MATCHES "Coverage")
      message(STATUS "Building for coverage analysis; skipping the python-tox-tests")
    elseif (TOX_EXE)
      configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini.in"
        "${CMAKE_CURRENT_BINARY_DIR}/tox.ini")
      add_test (NAME python-tox-test
                COMMAND ${env_py}
                  "PATH=${py_path}"
                  "CLASSPATH=${CMAKE_BINARY_DIR}/proton-j/proton-j.jar"
                  "SASLPASSWD=${SASLPASSWD_EXE}"
                  "SWIG=${SWIG_EXECUTABLE}"
                  ${VALGRIND_ENV}
                  ${TOX_EXE})
      set_tests_properties(python-tox-test
                           PROPERTIES
                             PASS_REGULAR_EXPRESSION "Totals: .* ignored, 0 failed"
                             FAIL_REGULAR_EXPRESSION "ERROR:[ ]+py[0-9]*: commands failed")
    else (TOX_EXE)
      message(STATUS "The tox tool is not available; skipping the python-tox-tests")
    endif ()
  endif (CMAKE_SYSTEM_NAME STREQUAL Linux AND TOX_TEST)

  set (perf_pythonpath "${py_pythonpath}" "${CMAKE_SOURCE_DIR}/examples/cpp")
  to_native_path ("${perf_pythonpath}" perf_pythonpath)
  add_custom_target(quick_perf_c ${env_py} -- "PATH=${py_path}" "PYTHONPATH=${perf_pythonpath}"
                           ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/perf/quick_perf.py" "C")
  add_dependencies(quick_perf_c reactor-send reactor-recv)

  add_custom_target(quick_perf_py COMMAND ${env_py} --
                    "PATH=${py_path}" "PYTHONPATH=${perf_pythonpath}"
                    ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/perf/quick_perf.py" "PYTHON"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/tools/apps/python")
  add_dependencies(quick_perf_py reactor-recv _cproton)

endif (BUILD_PYTHON)

find_program(RUBY_EXE "ruby")
if (RUBY_EXE AND BUILD_RUBY)
  set (rb_root "${pn_test_root}/ruby")
  set (rb_src "${CMAKE_CURRENT_SOURCE_DIR}/bindings/ruby")
  set (rb_lib "${CMAKE_CURRENT_SOURCE_DIR}/bindings/ruby/lib")
  set (rb_bin "${CMAKE_CURRENT_BINARY_DIR}/bindings/ruby")
  set (rb_bld "$<TARGET_FILE_DIR:qpid-proton>")
  set (rb_path $ENV{PATH} ${rb_bin} ${rb_bld})
  set (rb_rubylib ${rb_root} ${rb_src} ${rb_bin} ${rb_bld} ${rb_lib})
  to_native_path("${rb_path}" rb_path)
  to_native_path("${rb_rubylib}" rb_rubylib)

  # ruby example tests have no dependencies other than standard ruby.
  add_test(NAME ruby-example-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples/ruby
    COMMAND ${env_py} -- "PATH=${rb_path}" "RUBYLIB=${rb_rubylib}"
    ${RUBY_EXE} example_test.rb -v)

  # ruby unit tests:  tests/ruby/proton-test
  # only enable the tests if the Ruby gem dependencies were found
  if (DEFAULT_RUBY_TESTING)
    add_test (NAME ruby-unit-test
              COMMAND ${env_py} "PATH=${rb_path}" "RUBYLIB=${rb_rubylib}"
                      "${rb_root}/proton-test")

    # ruby spec tests
    find_program(RSPEC_EXE rspec)
    if (RSPEC_EXE)
      add_test (NAME ruby-spec-test
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/ruby
                COMMAND ${env_py} "PATH=${rb_path}" "RUBYLIB=${rb_rubylib}"
                        ${RSPEC_EXE})

    else(RSPEC_EXE)
      message (STATUS "Cannot find rspec, skipping rspec tests")
    endif(RSPEC_EXE)
  else (DEFAULT_RUBY_TESTING)
    message(STATUS "Skipping Ruby tests: missing dependencies")
  endif (DEFAULT_RUBY_TESTING)
else (RUBY_EXE)
  message (STATUS "Cannot find ruby, skipping ruby tests")
endif()

mark_as_advanced (RUBY_EXE RSPEC_EXE)

if (BUILD_JAVASCRIPT)
  add_test (javascript-codec ${env_py} node ${pn_test_root}/javascript/codec.js)
  add_test (javascript-message ${env_py} node ${pn_test_root}/javascript/message.js)
endif (BUILD_JAVASCRIPT)
