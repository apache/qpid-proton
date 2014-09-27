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

# Set default build type. Must come before project() which sets default to ""
set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE string
  "Build type: Debug, Release, RelWithDebInfo or MinSizeRel (default RelWithDebInfo)")
if (CMAKE_BUILD_TYPE MATCHES "Deb")
  set (has_debug_symbols " (has debug symbols)")
endif (CMAKE_BUILD_TYPE MATCHES "Deb")
message("Build type is \"${CMAKE_BUILD_TYPE}\"${has_debug_symbols}")

option(BUILD_WITH_CXX "Compile Proton using C++" OFF)
if ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
  # No C99 capability, use C++
  set(BUILD_WITH_CXX ON)
endif ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")

if (BUILD_WITH_CXX)
  project (Proton C CXX)
else (BUILD_WITH_CXX)
  project (Proton C)
endif (BUILD_WITH_CXX)

file(READ version.txt PN_VERSION_FILE)
string(REGEX MATCHALL "[0-9]+" PN_VERSION_LIST "${PN_VERSION_FILE}")

list(GET PN_VERSION_LIST 0 PN_VERSION_MAJOR)
list(GET PN_VERSION_LIST 1 PN_VERSION_MINOR)

set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")
message(STATUS "PN_VERSION: ${PN_VERSION}")

enable_testing()
include (CTest)
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

set (BINDING_LANGS PERL PHP PYTHON RUBY)

foreach (LANG ${BINDING_LANGS})
  set (SYSINSTALL_${LANG} OFF CACHE BOOL "Install ${LANG} bindings into interpreter specified location.")
  if (SYSINSTALL_BINDINGS OR SYSINSTALL_${LANG})
    set (CHECK_SYSINSTALL_${LANG} ON)
  else ()
    set (CHECK_SYSINSTALL_${LANG} OFF)
  endif ()
endforeach()

if (WIN32)
  set (EXAMPLES_INSTALL_DIR proton/examples)
endif (WIN32)

if (UNIX)
  set (EXAMPLES_INSTALL_DIR ${SHARE_INSTALL_DIR}/proton/examples)
endif (UNIX)

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})
# End of variables used during install

# Pull in local cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules/")

if (NOBUILD_JAVA)
  set (DEFAULT_JAVA OFF)
else()
  find_package(Java)
  if (JAVA_FOUND)
    set (DEFAULT_JAVA ON)
  else()
    set (DEFAULT_JAVA OFF)
  endif()
endif()

option (BUILD_JAVA "Build proton-j." ${DEFAULT_JAVA})

if (BUILD_JAVA)
  add_subdirectory(proton-j)
endif()

add_subdirectory(proton-c)

install (FILES LICENSE README TODO
         DESTINATION ${PROTON_SHARE})

# install the C messenger example
install (FILES examples/messenger/c/CMakeLists.txt
               examples/messenger/c/send.c
               examples/messenger/c/recv.c
         DESTINATION ${EXAMPLES_INSTALL_DIR}/messenger)

install (FILES examples/include/pncompat/misc_defs.h
               examples/include/pncompat/misc_funcs.inc
         DESTINATION ${EXAMPLES_INSTALL_DIR}/messenger/pncompat)

install (FILES examples/include/pncompat/internal/LICENSE
               examples/include/pncompat/internal/getopt.c
               examples/include/pncompat/internal/getopt.h
         DESTINATION ${EXAMPLES_INSTALL_DIR}/messenger/pncompat/internal)

# add relevant CTest support
find_program (MAVEN_EXE mvn DOC "Location of the maven program")
mark_as_advanced (MAVEN_EXE)
if (JAVA_FOUND AND MAVEN_EXE)
  add_test (proton-java mvn test --file ${CMAKE_CURRENT_SOURCE_DIR}/pom.xml)
else (JAVA_FOUND AND MAVEN_EXE)
  message (STATUS "Cannot find both Java and Maven: testing disabled for Proton-J")
endif (JAVA_FOUND AND MAVEN_EXE)

# Generate test environment settings
configure_file(${CMAKE_SOURCE_DIR}/config.sh.in
               ${CMAKE_BINARY_DIR}/config.sh @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/config.bat.in
               ${CMAKE_BINARY_DIR}/config.bat @ONLY)
