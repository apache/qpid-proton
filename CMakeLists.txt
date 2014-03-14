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

## LANGUAGE BINDINGS
#  If ASK_$LANG is 1 then the language is queried for the directory into which
#  those language bindings are to be installed. If it is 0 then the bindings
#  are installed to $CMAKE_INSTALL_PREFIX/bindings/$LANG
#
set (BINDINGS_DIR bindings CACHE PATH "Default directory for language bindings")
set (ASK_PERL 0 CACHE INTEGER "Ask Perl for install directories")
set (ASK_PHP 0 CACHE INTEGER "Ask PHP for install directories")
set (ASK_PYTHON 0 CACHE INTEGER "Ask Python for install directories")
set (ASK_RUBY 0 CACHE INTEGER "Ask Ruby for install directories")
set (ASK_ALL 0 CACHE INTEGER "If 1 then ask all languages for their directory")

if (ASK_ALL)
  set (ASK_PERL 1)
  set (ASK_PHP 1)
  set (ASK_PYTHON 1)
  set (ASK_RUBY 1)
endif (ASK_ALL)

message(STATUS "PYTHON_ARCHLIB_DIR=${PYTHON_ARCHLIB_DIR}")

if (WIN32)
  set (EXAMPLES_INSTALL_DIR proton/examples)
endif (WIN32)

if (UNIX)
  set (EXAMPLES_INSTALL_DIR ${SHARE_INSTALL_DIR}/proton/examples)
endif (UNIX)

set (JAVA_INSTALL_DIR ${SHARE_INSTALL_DIR}/java CACHE PATH "Installation directory for all JARs except those using JNI")
set (JNI_INSTALL_DIR ${LIB_INSTALL_DIR}/java CACHE PATH "Installation directory for all JARs utilising JNI")
set (JNI_SHARED_LIB_INSTALL_DIR ${LIB_INSTALL_DIR} CACHE PATH "Installation directory for shared objects used by JNI JARs")

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})
# End of variables used during install

set (PROTON_JAR_DEPEND_DIR /usr/share/java/ CACHE PATH
      "When locating compile-time dependencies, the build system searches this location in addition to the default ones provided by find_jar")

# Pull in local cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules/")

set (java_default ON)
option(GEN_JAVA "Include java-related artifacts" ${java_default})
if (GEN_JAVA)
  find_package( Java )
  if (JAVA_FOUND)
    message(STATUS "Java version: ${Java_VERSION}. javac is at: ${Java_JAVAC_EXECUTABLE}")
    include(UseJava)
    include(ProtonUseJava)

    add_subdirectory(proton-j)
  endif()
endif()

add_subdirectory(proton-c)

install (FILES LICENSE README TODO
         DESTINATION ${PROTON_SHARE})

# install the C messenger example
install (FILES examples/messenger/c/CMakeLists.txt
               examples/messenger/c/send.c
               examples/messenger/c/recv.c
         DESTINATION ${EXAMPLES_INSTALL_DIR}/messenger)

install (FILES examples/include/pncompat/internal/LICENSE
               examples/include/pncompat/internal/getopt.c
               examples/include/pncompat/internal/getopt.h
               examples/include/pncompat/misc_defs.h
               examples/include/pncompat/misc_funcs.inc
         DESTINATION ${EXAMPLES_INSTALL_DIR}/messenger/pncompat)

# add relevant CTest support
find_program (MAVEN_EXECUTABLE mvn DOC "Location of the maven program")
if (JAVA_FOUND AND MAVEN_EXECUTABLE)
  add_test (proton-java mvn test --file ${CMAKE_CURRENT_SOURCE_DIR}/pom.xml)
else (JAVA_FOUND AND MAVEN_EXECUTABLE)
  message (STATUS "Cannot find both Java and Maven: testing disabled for Proton-J")
endif (JAVA_FOUND AND MAVEN_EXECUTABLE)
