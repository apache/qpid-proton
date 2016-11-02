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

set (Proton_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set (ProtonCpp_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set result to a native search path
macro(set_search_path result)  # args after result are directories or search paths.
  set(${result} ${ARGN})
  if (UNIX)
    string(REPLACE ";" ":" ${result} "${${result}}") # native search path separators.
  endif()
  file(TO_NATIVE_PATH "${${result}}" ${result}) # native slash separators
endmacro()

# Some non-python examples use exampletest.py to drive their self-tests.
set_search_path(EXAMPLE_PYTHONPATH "${CMAKE_CURRENT_SOURCE_DIR}" "$ENV{PYTHON_PATH}")
set(EXAMPLE_ENV "PYTHONPATH=${EXAMPLE_PYTHONPATH}")

add_subdirectory(c)
add_subdirectory(go)
if (BUILD_CPP)
  add_subdirectory(cpp)
endif()
