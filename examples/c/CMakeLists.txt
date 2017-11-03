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
cmake_minimum_required (VERSION 2.8.12)

find_package(Proton REQUIRED Core Proactor)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

include_directories(${Proton_INCLUDE_DIRS})
add_definitions(${Proton_DEFINITIONS})

foreach (name broker send receive direct send-abort send-ssl)
  add_executable(c-${name} ${name}.c)
  target_link_libraries(c-${name} ${Proton_Proactor_LIBRARIES} ${Proton_Core_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
  set_target_properties(c-${name} PROPERTIES
    OUTPUT_NAME ${name})
endforeach()


# Add a test to run all examples

# Make correct environment to find test executables and valgrind.
if(WIN32)
  set(test_path "$<TARGET_FILE_DIR:c-broker>;$<TARGET_FILE_DIR:qpid-proton-core>;$<TARGET_FILE_DIR:qpid-proton-proactor>")
else()
  set(test_path "$<TARGET_FILE_DIR:c-broker>:$ENV{PATH}")
endif()

set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py ${EXAMPLE_ENV})
add_test(
  NAME c-example-tests
  COMMAND ${run_env} "PATH=${test_path}"  ${VALGRIND_ENV} -- ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
