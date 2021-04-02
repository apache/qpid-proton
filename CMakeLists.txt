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

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules")
set (CMAKE_THREAD_PREFER_PTHREAD TRUE)

include (CTest)
include (CheckLanguage)
include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckPythonModule)

find_package (OpenSSL)
find_package (Threads)
find_package (SWIG)
find_package (CyrusSASL)

# This setting mirrors FindPythonInterp behavior on Windows/macOS, which did not consult registry/frameworks first.`
if (NOT DEFINED Python_FIND_REGISTRY)
  set(Python_FIND_REGISTRY "LAST")
endif ()
if (NOT DEFINED Python_FIND_FRAMEWORK)
  set(Python_FIND_FRAMEWORK "LAST")
endif ()
find_package(Python
        REQUIRED COMPONENTS Interpreter
        OPTIONAL_COMPONENTS Development)

include(tests/PNAddTest.cmake)

# Set up runtime checks (valgrind, sanitizers etc.)
include(tests/RuntimeCheck.cmake)

## Set up so that find_package(Proton) & find_package(ProtonCpp) work in the build tree
set (Proton_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/config)
set (ProtonCpp_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/config)

## Variables used across components

set (PN_ENV_SCRIPT "${Python_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/env.py")
set (PN_C_INCLUDE_DIR "${CMAKE_BINARY_DIR}/c/include")
set (PN_C_LIBRARY_DIR "${CMAKE_BINARY_DIR}/c")
set (PN_C_SOURCE_DIR "${CMAKE_BINARY_DIR}/c/src")

## C
if (DEFINED CMAKE_C_COMPILE_FEATURES)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_EXTENSIONS OFF)
  set(CMAKE_C_STANDARD_REQUIRED ON)
else()
  set (C_STANDARD_GNU -std=c99)
  set (C_EXTENDED_GNU -std=gnu99)
  set (C_STANDARD_Clang -std=c99)
  set (C_EXTENDED_Clang -std=gnu99)
endif()

set (C_STANDARD_FLAGS ${C_STANDARD_${CMAKE_C_COMPILER_ID}})
set (C_EXTENDED_FLAGS ${C_EXTENDED_${CMAKE_C_COMPILER_ID}})

## C++
check_language (CXX)
if (CMAKE_CXX_COMPILER)
  enable_language(CXX)

  # This effectively checks for cmake version 3.1 or later
  if (DEFINED CMAKE_CXX_COMPILE_FEATURES)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_EXTENSIONS OFF)
    # Every supported version of Visual Studio (2015 onwards) supports enough C++11 that we
    # no longer need to run the individual feature tests
  else ()
    include(CheckCXXCompilerFlag)
    # These flags work with GCC/Clang/SunPro compilers
    check_cxx_compiler_flag("-std=c++11" ACCEPTS_CXX11)
    check_cxx_compiler_flag("-std=c++0x" ACCEPTS_CXX0X)
    if (ACCEPTS_CXX11)
      set(CXX_STANDARD "-std=c++11")
    elseif(ACCEPTS_CXX0X)
      set(CXX_STANDARD "-std=c++0x")
    endif()
  endif ()
endif()

# Build static C and C++ libraries in addition to shared libraries.
option(BUILD_STATIC_LIBS "Build static libraries as well as shared libraries" OFF)

if (CMAKE_CONFIGURATION_TYPES)
  # There is no single "build type"...
  message(STATUS "Build types are ${CMAKE_CONFIGURATION_TYPES}")
else (CMAKE_CONFIGURATION_TYPES)
  # There is a single build configuration
  # If the build type is not set then set the default
  if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
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

# Try to keep any platform specific overrides together here:

# MacOS has a bunch of differences in build tools and process and so we have to turn some things
# off if building there:
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set (NOENABLE_WARNING_ERROR ON)
  set (LIB_SUFFIX "")
  if (POLICY CMP0042)
    cmake_policy (SET CMP0042 NEW)
  endif ()
endif ()

# Benchmarks should be disabled by default
set (NOENABLE_BENCHMARKS ON)

# Add options here called <whatever> they will turn into "ENABLE_<whatever" and can be
# overridden on a platform specific basis above by NOENABLE_<whatever>
set (OPTIONS WARNING_ERROR UNDEFINED_ERROR LINKTIME_OPTIMIZATION HIDE_UNEXPORTED_SYMBOLS FUZZ_TESTING BENCHMARKS)

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
option(ENABLE_FUZZ_TESTING "Enable building fuzzers and regression testing with libFuzzer" ${DEFAULT_FUZZ_TESTING})
option(ENABLE_BENCHMARKS "Enable building and running benchmarks with Google Benchmark" ${DEFAULT_BENCHMARKS})

option(BUILD_EXAMPLES "Enable building example programs" ON)

# Set any additional compiler specific flags
set (LTO_GNU "-flto -fno-fat-lto-objects")
set (LTO_Clang "-flto=thin")
set (LTO_MSVC "/GL")
set (LINK_LTO_GNU "-flto -fno-fat-lto-objects")
set (LINK_LTO_Clang "-flto=thin")
set (LINK_LTO_MSVC "/LTCG")

set (WERROR_GNU "-Werror")
set (WERROR_Clang "-Werror")
set (WERROR_MSVC "")

set (COMMON_WARNING_GNU "-Wall -pedantic-errors")
set (COMMON_WARNING_Clang "-Wall -pedantic")
set (COMMON_WARNING_MSVC "")

set (CC_WARNING_GNU "-Wno-unused-parameter -Wstrict-prototypes -Wvla -Wsign-compare -Wwrite-strings")
set (CC_WARNING_Clang "-Wno-unused-parameter -Wstrict-prototypes -Wvla -Wsign-compare -Wwrite-strings")
set (CC_WARNING_MSVC "/wd4244 /wd4267 /wd4800 /wd4996")

set (CXX_WARNING_GNU "")
# TODO aconway 2016-01-06: we should be able to clean up the code and turn on
# some of these warnings.
#set (CXX_WARNING_Clang "-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-float-equal -Wno-padded -Wno-sign-conversion -Wno-switch-enum -Wno-weak-vtables -Wno-exit-time-destructors -Wno-global-constructors -Wno-shorten-64-to-32 -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast -Wno-missing-noreturn")
set (CXX_WARNING_Clang "")
set (CXX_WARNING_MSVC "")

set (CATCH_UNDEFINED_Linux "-Wl,--no-undefined")
set (CATCH_UNDEFINED_FreeBSD "-Wl,--no-undefined")
set (CATCH_UNDEFINED_Darwin "-Wl,-undefined,error")
set (CATCH_UNDEFINED_Windows "")

set (HIDE_SYMBOLS_GNU "-fvisibility=hidden")
set (HIDE_SYMBOLS_Clang "-fvisibility=hidden")
set (HIDE_SYMBOLS_SunPro "-xldscope=hidden")

if (ENABLE_LINKTIME_OPTIMIZATION)
  set (LTO ${LTO_${CMAKE_C_COMPILER_ID}})
  set (LINK_LTO ${LINK_LTO_${CMAKE_C_COMPILER_ID}})
endif (ENABLE_LINKTIME_OPTIMIZATION)

if (ENABLE_WARNING_ERROR)
  set (WERROR ${WERROR_${CMAKE_C_COMPILER_ID}})
endif (ENABLE_WARNING_ERROR)

if (ENABLE_UNDEFINED_ERROR)
  set (CATCH_UNDEFINED ${CATCH_UNDEFINED_${CMAKE_SYSTEM_NAME}})
endif (ENABLE_UNDEFINED_ERROR)

if (ENABLE_HIDE_UNEXPORTED_SYMBOLS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${HIDE_SYMBOLS_${CMAKE_C_COMPILER_ID}}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${HIDE_SYMBOLS_${CMAKE_C_COMPILER_ID}}")
endif (ENABLE_HIDE_UNEXPORTED_SYMBOLS)

set (COMMON_WARNING_FLAGS ${COMMON_WARNING_${CMAKE_C_COMPILER_ID}})
set (CXX_WARNING_FLAGS "${WERROR} ${COMMON_WARNING_FLAGS} ${CXX_WARNING_${CMAKE_C_COMPILER_ID}}")
set (COMPILE_WARNING_FLAGS "${WERROR} ${COMMON_WARNING_FLAGS} ${CC_WARNING_${CMAKE_C_COMPILER_ID}}")

if (MSVC)
  set(CMAKE_DEBUG_POSTFIX "d")
endif (MSVC)

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

macro (pn_absolute_install_dir NAME VALUE PREFIX)
  if (IS_ABSOLUTE ${VALUE})
    set (${NAME} "${VALUE}")
  elseif (IS_ABSOLUTE ${PREFIX})
    set (${NAME} "${PREFIX}/${VALUE}")
  else ()
    set (${NAME} "${CMAKE_BINARY_DIR}/${PREFIX}/${VALUE}")
  endif ()
  get_filename_component (${NAME} ${${NAME}} ABSOLUTE)
endmacro ()

pn_absolute_install_dir (PREFIX "." ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir (EXEC_PREFIX "." ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir (LIBDIR ${LIB_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})
pn_absolute_install_dir (INCLUDEDIR ${INCLUDE_INSTALL_DIR} ${CMAKE_INSTALL_PREFIX})

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

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton)
# End of variables used during install

# Set result to a native search path - used by examples and binding tests.
# args after result are directories or search paths.
macro(set_search_path result)
  set(${result} ${ARGN})
  if (UNIX)
    string(REPLACE ";" ":" ${result} "${${result}}") # native search path separators.
  endif()
  file(TO_NATIVE_PATH "${${result}}" ${result}) # native slash separators
endmacro()

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # CMake uses semicolon as list separator. Change ';'->'\;'
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    string (REPLACE ";" "\;" path "${path}")
    # without this, ...;last\dir\ would later combine with list separator ; into \;
    set (${result} "${path}\;" PARENT_SCOPE)
  endfunction()
else (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # Just change ';'->':'
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    string (REPLACE ";" ":" path "${path}")
    set (${result} "${path}" PARENT_SCOPE)
  endfunction()
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

add_custom_target(docs)
add_custom_target(doc DEPENDS docs)

# Note: Process bindings after the source lists have been defined so
# the bindings can reference them.
add_subdirectory(c)

# Add bindings that do not require swig here - the directory name must be the same as the binding name
# See below for swig bindings
set(BINDINGS cpp go)

if (CMAKE_CXX_COMPILER)
  set (DEFAULT_CPP ON)
endif()

# Prerequisites for Go
find_program(GO_EXE go)
mark_as_advanced(GO_EXE)
if (GO_EXE)
  set (DEFAULT_GO ON)
  if(WIN32 OR RUNTIME_CHECK)
    # Go on windows requires gcc tool chain
    # Go does not work with C-based runtime checkers.
    set (DEFAULT_GO OFF)
  endif()
endif (GO_EXE)

if(SWIG_FOUND)
  # Add any new swig bindings here - the directory name must be the same as the binding name
  list(APPEND BINDINGS python ruby)

  if (POLICY CMP0078)
    cmake_policy(SET CMP0078 OLD)
  endif()
  if (POLICY CMP0086)
    cmake_policy(SET CMP0086 OLD)
  endif()
  include(UseSWIG)

  # All swig modules should include ${PROTON_HEADERS} in SWIG_MODULE_<name>_EXTRA_DEPS
  file(GLOB PROTON_HEADERS "${CMAKE_SOURCE_DIR}/c/include/proton/*.h")
  # All swig modules should include ${BINDING_DEPS} or ${BINDING_DEPS_FULL} in swig_link_libraries
  set (BINDING_DEPS qpid-proton-core)
  set (BINDING_DEPS_FULL qpid-proton)

  # Add a block here to detect the prerequisites to build each language binding:
  #
  # If the prerequisites for the binding are present set a variable called
  # DEFAULT_{uppercase name of binding} to ON

  # Prerequisites for Python wrapper:
  if (Python_Development_FOUND)
    set (DEFAULT_PYTHON ON)
  endif ()

  # Prerequisites for Ruby:
  find_package(Ruby)
  if (RUBY_FOUND)
    set (DEFAULT_RUBY ON)
  endif ()
endif()

# To kick-start a build with just a few bindings enabled by default, e.g. ruby and go:
#
#     cmake -DBUILD_BINDINGS="ruby;go"
#
# This is only used when CMakeCache.txt is first created, after that set the normal
# BUILD_XXX variables to enable/disable bindings.
#
if (NOT DEFINED BUILD_BINDINGS)
    set(BUILD_BINDINGS "${BINDINGS}")
endif()

foreach(BINDING ${BINDINGS})
  string(TOUPPER ${BINDING} UBINDING)
  list(FIND BUILD_BINDINGS ${BINDING} N)
  if(NOBUILD_${UBINDING} OR ( N EQUAL -1 ) ) # Over-ridden or not on the BUILD_BINDINGS list
    set(DEFAULT_${UBINDING} OFF)
  endif()
  option(BUILD_${UBINDING} "Build ${BINDING} language binding" ${DEFAULT_${UBINDING}})
  if (BUILD_${UBINDING})
      add_subdirectory(${BINDING})
  endif ()
endforeach(BINDING)

unset(BUILD_BINDINGS CACHE) # Remove from cache, only relevant when creating the initial cache.

install (FILES LICENSE.txt README.md tests/share/CMakeLists.txt DESTINATION ${PROTON_SHARE})
install (FILES tests/share/examples-README.md RENAME README.md DESTINATION ${PROTON_SHARE}/examples)
install (DIRECTORY tests DESTINATION ${PROTON_SHARE} PATTERN share EXCLUDE)

# Generate test environment settings
configure_file(${CMAKE_SOURCE_DIR}/misc/config.sh.in
               ${CMAKE_BINARY_DIR}/config.sh @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/misc/config.bat.in
               ${CMAKE_BINARY_DIR}/config.bat @ONLY)

if (BUILD_EXAMPLES)
  add_subdirectory(tests/examples)
endif (BUILD_EXAMPLES)
