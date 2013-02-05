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

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 3)
set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")

# Start of variables used during install
set (INCLUDE_INSTALL_DIR include CACHE PATH "Include file directory")
set (LIB_INSTALL_DIR "lib${LIB_SUFFIX}" CACHE PATH "Library object file directory")
set (SYSCONF_INSTALL_DIR etc CACHE PATH "System read only configuration directory")
set (SHARE_INSTALL_DIR share CACHE PATH "Shared read only data directory")
set (MAN_INSTALL_DIR share/man CACHE PATH "Manpage directory")

set (PROTON_SHARE ${SHARE_INSTALL_DIR}/proton-${PN_VERSION})
# End of variables used during install

# Location of proton-api jar created by the build; used by proton-jni and proton-j-impl
set (PROTON_API_TARGET_JAR ${CMAKE_BINARY_DIR}/proton-j/proton-api/proton-api-${PN_VERSION}.jar)
set (PROTON_JAR_DEPEND_DIR /usr/share/java/ CACHE PATH
      "When locating compile-time dependencies, the build system searches this location in addition to the default ones provided by find_jar")

# Pull in local cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/tools/cmake/Modules/")

find_package( Java )
if (JAVA_FOUND)
  message("Java version: ${Java_VERSION}. javac is at: ${Java_JAVAC_EXECUTABLE}")
  include(UseJava)
  include(UseProtonJava)

  add_subdirectory(proton-j/proton-api)
  add_subdirectory(proton-j/proton)
endif()

add_subdirectory(proton-c)

install (FILES LICENSE README TODO
         DESTINATION ${PROTON_SHARE})

