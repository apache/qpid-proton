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

# This is a wrapper hack purely so that we can use FindPython
# with cmake 2.8.12 and its supplied older modules

if (POLICY CMP0094)  # https://cmake.org/cmake/help/latest/policy/CMP0094.html
    cmake_policy(SET CMP0094 NEW)  # FindPython should return the first matching Python on PATH
endif ()

if (DEFINED PYTHON_EXECUTABLE AND DEFINED Python_EXECUTABLE)
    message(FATAL_ERROR "Both PYTHON_EXECUTABLE and Python_EXECUTABLE are defined. Define at most one of those.")
endif ()

# FindPython was added in CMake 3.12, but there it always returned
#  newest Python on the entire PATH. We want to use the first one.
if (CMAKE_VERSION VERSION_LESS "3.15.0")
    if (DEFINED Python_EXECUTABLE)
        set(PYTHON_EXECUTABLE ${Python_EXECUTABLE})
    endif ()

    find_package (PythonInterp REQUIRED)
    # forward compatibility with FindPython
    set(Python_VERSION_STRING "${PYTHON_VERSION_STRING}")
    set(Python_EXECUTABLE "${PYTHON_EXECUTABLE}")
    # for completeness, these are not actually used now
    set(Python_VERSION_MAJOR "${PYTHON_VERSION_MAJOR}")
    set(Python_VERSION_MINOR "${PYTHON_VERSION_MINOR}")
    set(Python_VERSION_PATCH "${PYTHON_VERSION_PATCH}")

    find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT)
    set(Python_Development_FOUND "${PYTHONLIBS_FOUND}")
    set(Python_INCLUDE_DIRS "${PYTHON_INCLUDE_PATH}")
    set(Python_LIBRARIES "${PYTHON_LIBRARIES}")
else ()
    if (DEFINED PYTHON_EXECUTABLE)
        set(Python_EXECUTABLE ${PYTHON_EXECUTABLE})
    endif ()

    include(${CMAKE_ROOT}/Modules/FindPython.cmake)
endif ()
