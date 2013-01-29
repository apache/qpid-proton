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

project (Proton)

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 3)
set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")

# Location of proton-api jar created by the build; used by proton-jni and proton-j-impl
set (PROTON_API_TARGET_JAR ${CMAKE_BINARY_DIR}/proton-j/proton-api/proton-api-${PN_VERSION}.jar)
set (PROTON_JAR_DEPEND_DIR /usr/share/java/ CACHE PATH
      "When locating compile-time dependencies, the build system searches this location in addition to the default ones provided by find_jar")

# Used to allow the building of the Java bindings when CMAKE_VERSION < 2.6.8
if ( ${CMAKE_VERSION} VERSION_LESS "2.8.6" )
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
endif ()

find_package( Java )
if (JAVA_FOUND)
  message("Java version: ${Java_VERSION}. javac is at: ${Java_JAVAC_EXECUTABLE}")
  include(UseJava)
  include(UseProtonJava)

  add_subdirectory(proton-j/proton-api)
  add_subdirectory(proton-j/proton)
endif()

add_subdirectory(proton-c)

