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

include(${CMAKE_ROOT}/Modules/FindSWIG.cmake)

if (NOT COMMAND swig_add_library)
  macro (SWIG_ADD_LIBRARY name)
    set(options "")
    set(oneValueArgs LANGUAGE TYPE)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(_SAM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT DEFINED _SAM_LANGUAGE)
      message(FATAL_ERROR "SWIG_ADD_LIBRARY: Missing LANGUAGE argument")
    endif ()

    if (NOT DEFINED _SAM_SOURCES)
      message(FATAL_ERROR "SWIG_ADD_LIBRARY: Missing SOURCES argument")
    endif ()

    if (DEFINED _SAM_TYPE AND NOT _SAM_LANGUAGE STREQUAL "module")
      message(FATAL_ERROR "SWIG_ADD_LIBRARY: This fallback impl of swig_add_library supports the module type only")
    endif ()

    swig_add_module(${name} ${_SAM_LANGUAGE} ${_SAM_SOURCES})
  endmacro ()
endif (NOT COMMAND swig_add_library)

# Builtin FindSWIG.cmake "forgets" to make its outputs advanced like a good citizen
mark_as_advanced (SWIG_DIR SWIG_EXECUTABLE SWIG_VERSION)
