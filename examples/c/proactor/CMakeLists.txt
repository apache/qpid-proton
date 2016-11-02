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

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${Proton_INCLUDE_DIRS})

add_definitions(${COMPILE_WARNING_FLAGS} ${WERROR} ${COMPILE_PLATFORM_FLAGS} ${LINK_TIME_OPTIMIZATION})

find_package(Libuv)
if (Libuv_FOUND)
  foreach(name broker send receive)
    add_executable(libuv_${name} ${name}.c libuv_proactor.c)
    target_link_libraries(libuv_${name} ${Proton_LIBRARIES} ${Libuv_LIBRARIES})
    set_target_properties(libuv_${name} PROPERTIES
      COMPILE_DEFINITIONS  "PN_PROACTOR_INCLUDE=\"libuv_proactor.h\"")
  endforeach()

  # Add a test with the correct environment to find test executables and valgrind.
  if(WIN32)
    set(test_path "$<TARGET_FILE_DIR:libuv_broker>;$<TARGET_FILE_DIR:qpid-proton>")
  else(WIN32)
    set(test_path "${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}")
  endif(WIN32)
  set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py ${EXAMPLE_ENV} "PATH=${test_path}" ${VALGRIND_ENV})
  add_test(c-proactor-libuv ${run_env} -- ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test.py)
endif()
