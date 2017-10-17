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

find_package(Proton REQUIRED Core Proactor)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

include(CheckCCompilerFlag)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${Proton_INCLUDE_DIRS})

add_definitions(${C_EXAMPLE_FLAGS})

# Add a test with the correct environment to find test executables and valgrind.
if(WIN32)
  set(test_path "$<TARGET_FILE_DIR:broker>;$<TARGET_FILE_DIR:qpid-proton>")
else()
  set(test_path "${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}")
endif()

foreach (name broker send receive direct send-abort send-ssl)
  add_executable(c-${name} ${name}.c)
  target_link_libraries(c-${name} ${Proton_Proactor_LIBRARIES} ${Proton_Core_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
  set_target_properties(c-${name} PROPERTIES
    OUTPUT_NAME ${name})
endforeach()

set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py ${EXAMPLE_ENV} "PATH=${test_path}" ${VALGRIND_ENV})

# windows exclusion only for 0.18 beta
if(NOT WIN32)
add_test(c-example-tests ${run_env} -- ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v)
endif()
