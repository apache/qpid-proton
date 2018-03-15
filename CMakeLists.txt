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

cmake_minimum_required (VERSION 2.8.12)

project (Proton C)

include (CTest)
include (CheckLanguage)
include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckPythonModule)
include (c/src/versions.cmake)

enable_testing()

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules")
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package (OpenSSL)
find_package (Threads)
find_package (PythonInterp REQUIRED)

## Swig

find_package (SWIG)
mark_as_advanced (SWIG_DIR SWIG_EXECUTABLE SWIG_VERSION)

## Cyrus SASL

find_library (CYRUS_SASL_LIBRARY sasl2)
find_path (CYRUS_SASL_INCLUDE_DIR sasl/sasl.h PATH_SUFFIXES include)
find_package_handle_standard_args (CyrusSASL DEFAULT_MSG CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR)
find_program (SASLPASSWD_EXE saslpasswd2 DOC "Program used to make SASL user db for testing")
mark_as_advanced (CYRUS_SASL_LIBRARY CYRUS_SASL_INCLUDE_DIR SASLPASSWD_EXE)

## C++

check_language (CXX)

if (CMAKE_CXX_COMPILER)
  enable_language(CXX)
endif()

# TODO - Should change this test to take account of recent MSVC that does support C99
if (MSVC)
  # No C99 capability, use C++
  set(DEFAULT_BUILD_WITH_CXX ON)
endif (MSVC)

if (CMAKE_CXX_COMPILER)
  option(BUILD_WITH_CXX "Compile Proton using C++" ${DEFAULT_BUILD_WITH_CXX})
endif()

if (CMAKE_CONFIGURATION_TYPES)
  # There is no single "build type"...
  message(STATUS "Build types are ${CMAKE_CONFIGURATION_TYPES}")
else (CMAKE_CONFIGURATION_TYPES)
  # There is a single build configuration
  # If the build type is not set then set the default
  if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE string
       "Build type: Debug, Release, RelWithDebInfo, MinSizeRel or Coverage (default RelWithDebInfo)" FORCE)
  endif ()

  # Set up extra coverage analysis options for gcc and clang
  if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS_COVERAGE "-g -O0 --coverage")
    set (CMAKE_CXX_FLAGS_COVERAGE "-g -O0 --coverage")
    set (CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")
    set (CMAKE_MODULE_LINKER_FLAGS_COVERAGE "--coverage")
    set (CMAKE_SHARED_LINKER_FLAGS_COVERAGE "--coverage")
    mark_as_advanced(
      CMAKE_C_FLAGS_COVERAGE CMAKE_CXX_FLAGS_COVERAGE
      CMAKE_EXE_LINKER_FLAGS_COVERAGE CMAKE_MODULE_LINKER_FLAGS_COVERAGE
      CMAKE_SHARED_LINKER_FLAGS_COVERAGE)
  endif()

  if (CMAKE_BUILD_TYPE MATCHES "Deb")
    set (has_debug_symbols " (has debug symbols)")
  endif (CMAKE_BUILD_TYPE MATCHES "Deb")
  message(STATUS "Build type is \"${CMAKE_BUILD_TYPE}\"${has_debug_symbols}")
endif (CMAKE_CONFIGURATION_TYPES)

# Add coverage target if we're building for test coverage
if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  make_directory(coverage_results)
  add_custom_target(coverage
    WORKING_DIRECTORY ./coverage_results
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/record-coverage.sh ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  if (NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    set(LIB_SUFFIX "")
    set(CMAKE_MACOSX_RPATH ON)
  endif ()
endif ()

file(READ VERSION.txt PN_VERSION_FILE)
string(STRIP ${PN_VERSION_FILE} PN_VERSION_LINE)
string(REPLACE "-" ";" PN_VERSION_SPLIT "${PN_VERSION_LINE}")
list(GET PN_VERSION_SPLIT 0 PN_VERSION_CLEAN)
list(REMOVE_AT PN_VERSION_SPLIT 0)
string(REPLACE ";" "-" PN_VERSION_QUALIFIER "${PN_VERSION_SPLIT}")
string(REGEX MATCHALL "[0-9]+" PN_VERSION_LIST "${PN_VERSION_CLEAN}")

list(GET PN_VERSION_LIST 0 PN_VERSION_MAJOR)
list(GET PN_VERSION_LIST 1 PN_VERSION_MINOR)

list(LENGTH PN_VERSION_LIST PN_VERSION_LENGTH)
if (${PN_VERSION_LENGTH} GREATER 2)
  list(GET PN_VERSION_LIST 2 PN_VERSION_POINT)
  set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}.${PN_VERSION_POINT}")
else()
  set (PN_VERSION_POINT 0)
  set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")
endif()

message(STATUS "PN_VERSION: ${PN_VERSION} (${PN_VERSION_QUALIFIER})")

set (pn_test_root "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set (pn_test_bin "${CMAKE_CURRENT_BINARY_DIR}/tests")

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

# Start of variables used during install
set (INCLUDE_INSTALL_DIR include CACHE PATH "Include file directory")
set (LIB_INSTALL_DIR "lib${LIB_SUFFIX}" CACHE PATH "Library object file directory")
set (SYSCONF_INSTALL_DIR etc CACHE PATH "System read only configuration directory")
set (SHARE_INSTALL_DIR share CACHE PATH "Shared read only data directory")
set (MAN_INSTALL_DIR share/man CACHE PATH "Manpage directory")

mark_as_advanced (INCLUDE_INSTALL_DIR LIB_INSTALL_DIR SYSCONF_INSTALL_DIR SHARE_INSTALL_DIR MAN_INSTALL_DIR)

## LANGUAGE BINDINGS

# Default directory for language bindings not being installed into
# system specified locations.
set (BINDINGS_DIR ${LIB_INSTALL_DIR}/proton/bindings)

set (SYSINSTALL_BINDINGS OFF CACHE BOOL "If SYSINSTALL_BINDINGS is OFF then proton bindings will be installed underneath ${BINDINGS_DIR} and each user will need to modify their interpreter configuration to load the appropriate binding. If SYSINSTALL_BINDINGS is ON, then each language interpreter will be queried for the appropriate directory and proton bindings will be installed and available system wide with no additional per user configuration.")

set (BINDING_LANGS PYTHON RUBY)

foreach (LANG ${BINDING_LANGS})
  set (SYSINSTALL_${LANG} OFF CACHE BOOL "Install ${LANG} bindings into interpreter specified location.")
  if (SYSINSTALL_BINDINGS OR SYSINSTALL_${LANG})
    set (CHECK_SYSINSTALL_${LANG} ON)
  else ()
    set (CHECK_SYSINSTALL_${LANG} OFF)
  endif ()
endforeach()

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})
# End of variables used during install

# Can't do valgrind and coverage at athe same time - coverage takes precedence
if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  message(STATUS "Building for coverage analysis; no run-time error detection")
else ()
  # Check for valgrind here so tests under proton-c/ and examples/ can use it.
  find_program(VALGRIND_EXECUTABLE valgrind DOC "Location of the valgrind program")
  mark_as_advanced (VALGRIND_EXECUTABLE)

  option(ENABLE_VALGRIND "Use valgrind to detect run-time problems" ON)
  if (ENABLE_VALGRIND)
    if (VALGRIND_EXECUTABLE)
      set (VALGRIND_SUPPRESSIONS ${CMAKE_SOURCE_DIR}/c/src/tests/valgrind.supp CACHE STRING "Default valgrind suppressions")
      set (VALGRIND_OPTIONS "--error-exitcode=42 --quiet --leak-check=full --trace-children=yes" CACHE STRING "Default valgrind options")
      mark_as_advanced(VALGRIND_SUPPRESSIONS VALGRIND_OPTIONS)
      set (VALGRIND_ENV "VALGRIND=${VALGRIND_EXECUTABLE}" "VALGRIND_ARGS=${VALGRIND_OPTIONS} --suppressions=${VALGRIND_SUPPRESSIONS}")
      separate_arguments(VALGRIND_OPTIONS_LIST UNIX_COMMAND ${VALGRIND_OPTIONS})
      set (memcheck-cmd ${VALGRIND_EXECUTABLE} ${VALGRIND_OPTIONS_LIST} "--suppressions=${VALGRIND_SUPPRESSIONS}")
    else ()
      message(STATUS "Can't locate the valgrind command; no run-time error detection")
    endif ()
  endif ()
endif ()

# Options to enable sanitizing compile flags. Compile flags are set in proton-c/CMakeLists.txt
option(ENABLE_SANITIZERS "Compile with sanitizers (ASan, UBSan, TSan); incompatible with Valgrind" OFF)
option(ENABLE_TSAN "Compile with Thread Sanitizer (TSan); incompatible with Valgrind" OFF)
if (ENABLE_SANITIZERS OR ENABLE_TSAN)
  set(DISABLE ENABLE_VALGRIND ENABLE_UNDEFINED_ERROR BUILD_GO)
  message(STATUS "Building with sanitizers; disables ${DISABLE}")
  foreach(d ${DISABLE})
    set(${d} OFF CACHE BOOL "Disabled to run sanitizers" FORCE)
  endforeach()
  unset(VALGRIND_ENV)
endif()

# Set result to a native search path - used by examples and binding tests.
# args after result are directories or search paths.
macro(set_search_path result)
  set(${result} ${ARGN})
  if (UNIX)
    string(REPLACE ";" ":" ${result} "${${result}}") # native search path separators.
  endif()
  file(TO_NATIVE_PATH "${${result}}" ${result}) # native slash separators
endmacro()

add_custom_target(docs)
add_subdirectory(c/docs)

if(WIN32 AND NOT CYGWIN)
  # linking against Windows native libraries, including mingw
  set (PN_WINAPI TRUE)
endif(WIN32 AND NOT CYGWIN)

set(ssl_impl, none)
if(PN_WINAPI)
  set(ssl_impl schannel)
  set(ssl_providers "'none','schannel','openssl'")
else(PN_WINAPI)
  if (OPENSSL_FOUND AND Threads_FOUND)
    set(ssl_impl openssl)
  endif ()
  set(ssl_providers "'none','openssl'")
endif(PN_WINAPI)
set(SSL_IMPL ${ssl_impl} CACHE STRING "Library to use for SSL/TLS support. Valid values: ${ssl_providers}")

set(sasl_providers cyrus none)
if (CYRUSSASL_FOUND AND Threads_FOUND)
  set (sasl_impl cyrus)
else ()
  set (sasl_impl none)
endif ()
set(SASL_IMPL ${sasl_impl} CACHE STRING "Library to use for SASL support. Valid values: ${sasl_providers}")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/c/include/proton/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/c/include/proton/version.h"
  )

include_directories ("${CMAKE_CURRENT_BINARY_DIR}/c/src")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}/c/include")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/c/src")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/c/include")

set (env_py ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/env.py)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/c/src/encodings.h
  COMMAND ${env_py} PYTHONPATH=${CMAKE_SOURCE_DIR}/tools/python ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/c/src/encodings.h.py > ${CMAKE_CURRENT_BINARY_DIR}/c/src/encodings.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/c/src/encodings.h.py
  )

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/c/src/protocol.h
  COMMAND ${env_py} PYTHONPATH=${CMAKE_SOURCE_DIR}/tools/python ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/c/src/protocol.h.py > ${CMAKE_CURRENT_BINARY_DIR}/c/src/protocol.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/c/src/protocol.h.py
  )

add_custom_target(
  generated_c_files
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/c/src/protocol.h ${CMAKE_CURRENT_BINARY_DIR}/c/src/encodings.h
  )

# Select IO impl
if(PN_WINAPI)
  set (pn_io_impl c/src/reactor/io/windows/io.c c/src/reactor/io/windows/iocp.c c/src/reactor/io/windows/write_pipeline.c)
  set (pn_selector_impl c/src/reactor/io/windows/selector.c)
else(PN_WINAPI)
  set (pn_io_impl c/src/reactor/io/posix/io.c)
  set (pn_selector_impl c/src/reactor/io/posix/selector.c)
endif(PN_WINAPI)

# Link in SASL if present
if (SASL_IMPL STREQUAL cyrus)
  set(pn_sasl_impl c/src/sasl/sasl.c c/src/sasl/default_sasl.c c/src/sasl/cyrus_sasl.c)
  include_directories (${CYRUS_SASL_INCLUDE_DIR})
  set(SASL_LIB ${CYRUS_SASL_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
elseif (SASL_IMPL STREQUAL none)
  set(pn_sasl_impl c/src/sasl/sasl.c c/src/sasl/default_sasl.c c/src/sasl/cyrus_stub.c)
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
  set (pn_ssl_impl c/src/ssl/openssl.c)
  include_directories (${OPENSSL_INCLUDE_DIR})
  set (SSL_LIB ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
elseif (SSL_IMPL STREQUAL schannel)
  set (pn_ssl_impl c/src/ssl/schannel.c)
  set (SSL_LIB Crypt32.lib Secur32.lib)
else ()
  set (pn_ssl_impl c/src/ssl/ssl_stub.c)
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
    set (COMPILE_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wstrict-prototypes -Wc++-compat -Wvla -Wsign-compare -Wwrite-strings")
    set (COMPILE_LANGUAGE_FLAGS "-std=c99")
    set (COMPILE_PLATFORM_FLAGS "-std=gnu99")
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
  set (CXX_WARNING_FLAGS "${COMPILE_WARNING_FLAGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal -Wno-padded -Wno-sign-conversion -Wno-switch-enum -Wno-weak-vtables -Wno-exit-time-destructors -Wno-global-constructors -Wno-shorten-64-to-32 -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-missing-noreturn")
endif()

# Sanitizer flags apply to to both GNU and clang, C and C++
if(ENABLE_SANITIZERS)
  set(SANITIZE_FLAGS "-g -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak -fsanitize=undefined")
endif()
if(ENABLE_TSAN)
  set(SANITIZE_FLAGS "-g -fno-omit-frame-pointer -fsanitize=thread")
endif()
if (SANITIZE_FLAGS)
  mark_as_advanced(SANITIZE_FLAGS)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
  endif()
endif()

# Flags for example self-test build, CACHE INTERNAL for visibility
set(C_EXAMPLE_FLAGS "${COMPILE_WARNING_FLAGS} ${CMAKE_C_FLAGS}" CACHE INTERNAL "")
set(C_EXAMPLE_LINK_FLAGS "${SANITIZE_FLAGS}" CACHE INTERNAL "")

if (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_COMPILER_IS_GNUCC)
  # Ensure that examples build with c90, to deal with older c++03-as-c compilers.
  set(C_EXAMPLE_FLAGS "${C_EXAMPLE_FLAGS} -std=iso9899:1990 -pedantic")
endif()

if (MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(
        /wd4244
        /wd4267
        /wd4800
        /wd4996
    )
    set (qpid-proton-platform c/src/compiler/msvc/snprintf.c)
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

add_subdirectory(tools/test-apps/c tools/test-apps/c)

# for full source distribution:
set (qpid-proton-platform-all
  c/src/platform/platform.c
  c/src/reactor/io/windows/io.c
  c/src/reactor/io/windows/iocp.c
  c/src/reactor/io/windows/write_pipeline.c
  c/src/reactor/io/windows/selector.c
  c/src/reactor/io/posix/io.c
  c/src/reactor/io/posix/selector.c
  )

# platform specific library build:
set (qpid-proton-platform-io
  c/src/platform/platform.c
  ${pn_io_impl}
  ${pn_selector_impl}
  )

# for full source distribution:
set (qpid-proton-layers-all
  c/src/sasl/sasl.c
  c/src/sasl/default_sasl.c
  c/src/sasl/cyrus_sasl.c
  c/src/sasl/cyrus_stub.c
  c/src/ssl/openssl.c
  c/src/ssl/schannel.c
  c/src/ssl/ssl_stub.c
  )

# for current build system's environment:
set (qpid-proton-layers
  ${pn_sasl_impl}
  ${pn_ssl_impl}
  )

set (qpid-proton-core
  c/src/core/object/object.c
  c/src/core/object/list.c
  c/src/core/object/map.c
  c/src/core/object/string.c
  c/src/core/object/iterator.c
  c/src/core/object/record.c

  c/src/core/log.c
  c/src/core/util.c
  c/src/core/error.c
  c/src/core/buffer.c
  c/src/core/types.c

  c/src/core/framing.c

  c/src/core/codec.c
  c/src/core/decoder.c
  c/src/core/encoder.c

  c/src/core/dispatcher.c
  c/src/core/connection_driver.c
  c/src/core/engine.c
  c/src/core/event.c
  c/src/core/autodetect.c
  c/src/core/transport.c
  c/src/core/message.c
)

set (qpid-proton-include-generated
  ${CMAKE_CURRENT_BINARY_DIR}/c/src/encodings.h
  ${CMAKE_CURRENT_BINARY_DIR}/c/src/protocol.h
  ${CMAKE_CURRENT_BINARY_DIR}/c/include/proton/version.h
  )

set (qpid-proton-private-includes
  c/src/messenger/store.h
  c/src/messenger/subscription.h
  c/src/messenger/messenger.h
  c/src/messenger/transform.h
  c/src/ssl/ssl-internal.h
  c/src/sasl/sasl-internal.h
  c/src/core/autodetect.h
  c/src/core/log_private.h
  c/src/core/config.h
  c/src/core/encoder.h
  c/src/core/dispatch_actions.h
  c/src/core/engine-internal.h
  c/src/core/transport.h
  c/src/core/framing.h
  c/src/core/buffer.h
  c/src/core/util.h
  c/src/core/dispatcher.h
  c/src/core/data.h
  c/src/core/decoder.h
  c/src/core/max_align.h
  c/src/core/message-internal.h
  c/src/reactor/io/windows/iocp.h
  c/src/reactor/selector.h
  c/src/reactor/io.h
  c/src/reactor/reactor.h
  c/src/reactor/selectable.h
  c/src/platform/platform.h
  c/src/platform/platform_fmt.h
  c/src/proactor/proactor-internal.h
  )

set (qpid-proton-extra
  c/src/extra/url.c

  c/src/reactor/reactor.c
  c/src/reactor/handler.c
  c/src/reactor/connection.c
  c/src/reactor/acceptor.c
  c/src/reactor/selectable.c
  c/src/reactor/timer.c

  c/src/handlers/handshaker.c
  c/src/handlers/iohandler.c
  c/src/handlers/flowcontroller.c

  c/src/messenger/messenger.c
  c/src/messenger/subscription.c
  c/src/messenger/store.c
  c/src/messenger/transform.c
  )

set (qpid-proton-include
  c/include/proton/cid.h
  c/include/proton/codec.h
  c/include/proton/condition.h
  c/include/proton/connection.h
  c/include/proton/connection_driver.h
  c/include/proton/delivery.h
  c/include/proton/disposition.h
  c/include/proton/engine.h
  c/include/proton/error.h
  c/include/proton/event.h
  c/include/proton/import_export.h
  c/include/proton/link.h
  c/include/proton/listener.h
  c/include/proton/log.h
  c/include/proton/message.h
  c/include/proton/netaddr.h
  c/include/proton/object.h
  c/include/proton/proactor.h
  c/include/proton/sasl.h
  c/include/proton/sasl-plugin.h
  c/include/proton/session.h
  c/include/proton/ssl.h
  c/include/proton/terminus.h
  c/include/proton/transport.h
  c/include/proton/type_compat.h
  c/include/proton/types.h
)

set (qpid-proton-include-extra
  c/include/proton/handlers.h
  c/include/proton/messenger.h
  c/include/proton/reactor.h
  c/include/proton/selectable.h
  c/include/proton/url.h
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
    set (qpid-proton-proactor c/src/proactor/epoll.c c/src/proactor/proactor-internal.c)
    set (PROACTOR_LIBS -lpthread)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
      )
  endif()
endif()

if (PROACTOR STREQUAL "iocp" OR (NOT PROACTOR AND NOT PROACTOR_OK))
  if(WIN32 AND NOT CYGWIN)
    set (PROACTOR_OK iocp)
    set (qpid-proton-proactor c/src/proactor/win_iocp.c c/src/proactor/proactor-internal.c)
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
    set (qpid-proton-proactor c/src/proactor/libuv.c c/src/proactor/proactor-internal.c)
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
  VERSION   "${PN_LIB_CORE_VERSION}"
  SOVERSION "${PN_LIB_CORE_MAJOR_VERSION}"
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
  # When a Windows build creates c/src/encodings.h and c/src/protocol.h
  # only once then this can be removed.
  add_dependencies(qpid-proton qpid-proton-core)
endif (MSVC)

target_link_libraries (qpid-proton LINK_PRIVATE ${UUID_LIB} ${SSL_LIB} ${SASL_LIB} ${TIME_LIB} ${PLATFORM_LIBS} ${PROACTOR_LIBS})

set_target_properties (
  qpid-proton
  PROPERTIES
  VERSION   "${PN_LIB_LEGACY_VERSION}"
  SOVERSION "${PN_LIB_LEGACY_MAJOR_VERSION}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
  )

if (MSVC)
  # guard against use of C99 violating functions on Windows
  include(WindowsC99CheckDef)
endif(MSVC)

if (qpid-proton-proactor)
  # Bizarre CMake variable setting
  set(HAS_PROACTOR True)
  add_library (
    qpid-proton-proactor SHARED ${qpid-proton-proactor})
  target_link_libraries (qpid-proton-proactor  LINK_PUBLIC qpid-proton-core)
  target_link_libraries (qpid-proton-proactor  LINK_PRIVATE ${PLATFORM_LIBS} ${PROACTOR_LIBS})
  list(APPEND LIB_TARGETS qpid-proton-proactor)
  set_target_properties (
    qpid-proton-proactor
    PROPERTIES
    VERSION   "${PN_LIB_PROACTOR_VERSION}"
    SOVERSION "${PN_LIB_PROACTOR_MAJOR_VERSION}"
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
file(GLOB headers "c/include/proton/*.[hi]")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton)
install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/c/include/proton/version.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/c/src/lib${LIB}.pc.in
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
  ${CMAKE_CURRENT_SOURCE_DIR}/c/src/ProtonConfig.cmake.in
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

add_subdirectory(c/src/tests)

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

  if (CMAKE_BUILD_TYPE MATCHES "Coverage")
    set (python_coverage_options -m coverage run)
  endif(CMAKE_BUILD_TYPE MATCHES "Coverage")

  add_test (NAME python-test
            COMMAND ${env_py}
              "PATH=${py_path}" "PYTHONPATH=${py_pythonpath}"
              "SASLPASSWD=${SASLPASSWD_EXE}"
              ${VALGRIND_ENV}
              ${PYTHON_EXECUTABLE} -- ${python_coverage_options} "${py_root}/proton-test")
  set_tests_properties(python-test PROPERTIES PASS_REGULAR_EXPRESSION "Totals: .* 0 failed")

  # Eventually, we'll get rid of this check when other
  # platforms will be supported. Since `setup.py` will skip
  # the build for non linux plaforms, it doesn't make sense
  # to try to run them.
  option(TOX_TEST "Enable muti-version python testing with TOX" ON)
  set(tox_default "py26,py27,py33bbb,py34,py35")
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
        "${CMAKE_CURRENT_SOURCE_DIR}/misc/tox.ini.in"
        "${CMAKE_CURRENT_BINARY_DIR}/tox.ini")
      add_test (NAME python-tox-test
                COMMAND ${env_py}
                  "PATH=${py_path}"
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
endif (BUILD_PYTHON)

add_subdirectory(c/examples)

install (DIRECTORY c/examples/
         DESTINATION "${PROTON_SHARE}/examples/c"
         PATTERN ProtonConfig.cmake EXCLUDE)
install (FILES LICENSE.txt README.md
         DESTINATION ${PROTON_SHARE})

# Generate test environment settings
configure_file(${CMAKE_SOURCE_DIR}/misc/config.sh.in
               ${CMAKE_BINARY_DIR}/config.sh @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/misc/config.bat.in
               ${CMAKE_BINARY_DIR}/config.bat @ONLY)
