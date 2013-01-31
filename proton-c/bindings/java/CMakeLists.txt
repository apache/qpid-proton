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

SET(CMAKE_SWIG_FLAGS -package org.apache.qpid.proton.jni)
SET(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/src/main/java/org/apache/qpid/proton/jni)

message( "Using JNI libraries: ${JNI_LIBRARIES}" )

swig_add_module(proton-swig java java.i)
include_directories(${JNI_INCLUDE_DIRS})
swig_link_libraries(proton-swig ${BINDING_DEPS}  )

set_target_properties(proton-swig
    PROPERTIES
    PREFIX "lib")

add_custom_command(OUTPUT ${CMAKE_SWIG_OUTDIR} COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SWIG_OUTDIR} COMMENT "Creating target directory for Proton-JNI sources")
add_custom_target(protonjnioutdir ALL DEPENDS ${CMAKE_SWIG_OUTDIR})
add_dependencies(proton-swig protonjnioutdir)

set(CMAKE_JAVA_TARGET_VERSION ${PN_VERSION})

file(GLOB_RECURSE THUNKING_SOURCES_ABS "src/main/java/*.java")

set(CMAKE_JAVA_INCLUDE_PATH ${PROTON_API_TARGET_JAR})

add_jar(proton-jni ${CMAKE_SWIG_OUTDIR}/*.java ${THUNKING_SOURCES_ABS})

include(UseProtonJava)
rebuild_jar(proton-jni proton-jni-${PN_VERSION}.jar)

add_dependencies(proton-jni proton-api proton-swig)
