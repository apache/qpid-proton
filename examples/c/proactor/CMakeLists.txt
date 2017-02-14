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

find_package(Proton REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${Proton_INCLUDE_DIRS})

# Check if the proton library has a proactor implementation.
include(CheckFunctionExists)
include(CMakePushCheckState)
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${Proton_LIBRARIES})
check_function_exists(pn_proactor HAS_PROACTOR)
cmake_pop_check_state()

if (HAS_PROACTOR)

add_definitions(${COMPILE_WARNING_FLAGS} ${WERROR} ${COMPILE_PLATFORM_FLAGS} ${LINK_TIME_OPTIMIZATION})

# Add a test with the correct environment to find test executables and valgrind.
if(WIN32)
  set(test_path "$<TARGET_FILE_DIR:broker>;$<TARGET_FILE_DIR:qpid-proton>")
else(WIN32)
  set(test_path "${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}")
  set(PLATFORM_LIBS pthread)
endif(WIN32)

foreach(name broker send receive direct)
  add_executable(proactor-${name} ${name}.c)
  target_link_libraries(proactor-${name} ${Proton_LIBRARIES} ${PLATFORM_LIBS})
  set_target_properties(proactor-${name} PROPERTIES OUTPUT_NAME ${name})
endforeach()

set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py ${EXAMPLE_ENV} "PATH=${test_path}" ${VALGRIND_ENV})
add_test(c-proactor ${run_env} -- ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test.py -v)

endif()
