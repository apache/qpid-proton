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

message(STATUS "Using JNI libraries: ${JNI_LIBRARIES}" )

if (BUILD_WITH_CXX)
    SET_SOURCE_FILES_PROPERTIES(java.i PROPERTIES CPLUSPLUS ON)
endif (BUILD_WITH_CXX)

swig_add_module(proton-swig java java.i)
include_directories(${JNI_INCLUDE_DIRS})
swig_link_libraries(proton-swig ${BINDING_DEPS}  )

set_target_properties(proton-swig
    PROPERTIES
    OUTPUT_NAME "proton-jni"
    PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}")


add_custom_command(OUTPUT ${CMAKE_SWIG_OUTDIR}
                   COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SWIG_OUTDIR}
                   COMMENT "Creating target directory for Proton-JNI sources")
add_custom_target(proton-jni-swig-out-dir ALL DEPENDS ${CMAKE_SWIG_OUTDIR})
add_dependencies(proton-swig proton-jni-swig-out-dir)

set(CMAKE_JAVA_TARGET_VERSION ${PN_VERSION})

set(CMAKE_JAVA_INCLUDE_PATH ${PROTON_API_TARGET_JAR})

set(PROTON_JNI_SOURCE_FILE_LIST ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/proton_jni_source_filelist)
set(PROTON_JNI_ALL_SOURCE_DIRS "${CMAKE_SWIG_OUTDIR}:${CMAKE_CURRENT_SOURCE_DIR}/src/main/java")

add_custom_target(proton-jni-sourcefile-list ALL
            COMMAND ${CMAKE_COMMAND}
                -DJAVA_SOURCE_DIR_PATHS=${PROTON_JNI_ALL_SOURCE_DIRS}
                -DJAVA_SOURCE_FILE_LIST=${PROTON_JNI_SOURCE_FILE_LIST}
                -P ${CMAKE_MODULE_PATH}/ProtonUseJavaSourceFileList.cmake)
add_dependencies(proton-jni-sourcefile-list proton-swig)

add_jar_from_filelist(proton-jni ${PROTON_JNI_SOURCE_FILE_LIST})

include(ProtonUseJava)
rebuild_jar(proton-jni proton-jni-${PN_VERSION}.jar)

add_dependencies(proton-jni proton-api proton-jni-sourcefile-list)

install_jar(proton-jni ${JNI_INSTALL_DIR})
install(TARGETS proton-swig
        DESTINATION ${JNI_SHARED_LIB_INSTALL_DIR})
