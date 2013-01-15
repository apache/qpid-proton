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

include(UseJava)

project (Proton-JNI Java)

add_subdirectory(api)

SET(CMAKE_SWIG_FLAGS -package org.apache.qpid.proton.jni)
SET(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/jni/src/main/java/org/apache/qpid/proton/jni)

message( "JNI_LIBRARIES" ${JNI_LIBRARIES} )
message( "JAVA_AWT_LIBRARY" ${JAVA_AWT_LIBRARY} )


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

file(GLOB_RECURSE THUNKING_SOURCES_ABS "jni/src/main/java/*.java")

# TODO: Why do we need to explicitly set the classpath?  Why is the fact that we depend on
# protonapi not sufficient?
set(CMAKE_JAVA_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}/api/proton-api-${PN_VERSION}.jar)

# TODO: We are relying on shell expansion to find the swig generated sources. Surely there is a better way?
add_jar(proton-jni ${CMAKE_SWIG_OUTDIR}/*.java ${THUNKING_SOURCES_ABS})

message("CMAKE_CURRENT_BINARY_DIR " ${CMAKE_CURRENT_BINARY_DIR})
message("CMAKE_JAVA_CLASS_OUTPUT_PATH" ${CMAKE_JAVA_CLASS_OUTPUT_PATH})

# We need to rebuild the JAR to a) include the resources b) include the 'structural' directory entries that are
# excluded by Cmake's add_jar command.  The latter is required by Google Reflections.
add_custom_command(TARGET proton-jni POST_BUILD
                   COMMAND ${Java_JAR_EXECUTABLE} cf proton-jni-${PN_VERSION}.jar
                              -C ${PROJECT_SOURCE_DIR}/jni/src/main/resources META-INF
                              -C ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/proton-jni.dir/ org
                   COMMENT "Rebuilding JAR to include missing resources")

add_dependencies(proton-jni proton-api proton-swig)
