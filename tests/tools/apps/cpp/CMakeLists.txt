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

include_directories("${CMAKE_SOURCE_DIR}/examples/cpp")
add_executable(reactor_send_cpp reactor_send.cpp)

target_link_libraries(reactor_send_cpp qpid-proton qpid-proton-cpp)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # No change needed for windows already use correct separator
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    set (${result} ${path} PARENT_SCOPE)
  endfunction()
else (CMAKE_SYSTEM_NAME STREQUAL Windows)
  # Just change ';'->':'
  function(to_native_path path result)
    file (TO_NATIVE_PATH "${path}" path)
    string (REGEX REPLACE ";" ":" path "${path}")
    set (${result} ${path} PARENT_SCOPE)
  endfunction()
endif (CMAKE_SYSTEM_NAME STREQUAL Windows)

set (py_bld "$<TARGET_FILE_DIR:qpid-proton>" "$<TARGET_FILE_DIR:qpid-proton-cpp>") # For windows
set (app_path $<TARGET_FILE_DIR:reactor-recv> $<TARGET_FILE_DIR:reactor_send_cpp>)
set (py_path ${py_bld} ${app_path} $ENV{PATH})
to_native_path("${py_path}" py_path)
set (py_pythonpath "${CMAKE_SOURCE_DIR}/examples/cpp" $ENV{PYTHONPATH})
to_native_path ("${py_pythonpath}" py_pythonpath)
set (perf_pythonpath "${py_pythonpath}" "${CMAKE_SOURCE_DIR}/examples/cpp")
to_native_path ("${perf_pythonpath}" perf_pythonpath)

add_custom_target(quick_perf_cpp ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py --
                  "PATH=${py_path}" "PYTHONPATH=${perf_pythonpath}"
                  ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tests/perf/quick_perf.py" "CPP")

add_dependencies(quick_perf_cpp reactor_send_cpp reactor-recv qpid-proton-cpp)
