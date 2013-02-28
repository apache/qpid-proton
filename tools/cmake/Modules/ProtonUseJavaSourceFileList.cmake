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

#
# Produces a text file containing a list of java source files from one
# or more java source directories.  Produces output suitable for use
# with javac's @ source file argument.
#
# JAVA_SOURCE_DIR_PATHS - colon (:) separated string of java source directories
# JAVA_SOURCE_FILE_LIST - name of text file to write
#

if (JAVA_SOURCE_DIR_PATHS)
    string(REPLACE ":" ";" JAVA_SOURCE_DIR_PATHS_LIST ${JAVA_SOURCE_DIR_PATHS})
    message(STATUS "Java source paths: ${JAVA_SOURCE_DIR_PATHS}")

    set(_JAVA_GLOBBED_FILES)
    foreach(JAVA_SOURCE_DIR_PATH ${JAVA_SOURCE_DIR_PATHS_LIST})
        if (EXISTS "${JAVA_SOURCE_DIR_PATH}")
            file(GLOB_RECURSE _JAVA_GLOBBED_TMP_FILES "${JAVA_SOURCE_DIR_PATH}/*.java")
            if (_JAVA_GLOBBED_TMP_FILES)
                list(APPEND _JAVA_GLOBBED_FILES ${_JAVA_GLOBBED_TMP_FILES})
            endif ()
        else ()
            message(SEND_ERROR "FATAL: Java source path ${JAVA_SOURCE_DIR_PATH} doesn't exist")
        endif ()
    endforeach()

    set (_CHECK_STALE OFF)
    set(_GENERATE_FILE_LIST ON)
    if (EXISTS ${JAVA_SOURCE_FILE_LIST})
        set (_CHECK_STALE ON)
        set(_GENERATE_FILE_LIST OFF)
    endif ()

    set(_JAVA_SOURCE_FILES_SEPARATED)
    foreach(_JAVA_GLOBBED_FILE ${_JAVA_GLOBBED_FILES})
        if (_CHECK_STALE)
           if (${_JAVA_GLOBBED_FILE} IS_NEWER_THAN ${JAVA_SOURCE_FILE_LIST})
               set(_GENERATE_FILE_LIST ON)
           endif()
        endif()
        set(_JAVA_SOURCE_FILES_SEPARATED ${_JAVA_SOURCE_FILES_SEPARATED}${_JAVA_GLOBBED_FILE}\n)
    endforeach()

    if (_GENERATE_FILE_LIST)
       message(STATUS "Writing Java source file list to ${JAVA_SOURCE_FILE_LIST}")
       file(WRITE ${JAVA_SOURCE_FILE_LIST} ${_JAVA_SOURCE_FILES_SEPARATED})
    endif()
else ()
    message(SEND_ERROR "FATAL: Can't find JAVA_SOURCE_DIR_PATHS")
endif ()
