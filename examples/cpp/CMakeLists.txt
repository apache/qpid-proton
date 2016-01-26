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

find_package(ProtonCpp REQUIRED)

include_directories(${ProtonCpp_INCLUDE_DIRS})

foreach(example
    broker
    helloworld
    helloworld_direct
    simple_recv
    simple_send
    direct_recv
    direct_send
    client
    server
    server_direct
    recurring_timer
    connection_options
    queue_browser
    selected_recv
    ssl
    ssl_client_cert
    encode_decode)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} ${ProtonCpp_LIBRARIES})
  set_source_files_properties(${example}.cpp PROPERTIES COMPILE_FLAGS "${CXX_WARNING_FLAGS}")
endforeach()

add_subdirectory(engine)

set(env_py ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py)

function(set_test_path dir)
  if (WIN32)
    set(test_path "${dir}" "$<TARGET_FILE_DIR:qpid-proton>" "$<TARGET_FILE_DIR:qpid-proton-cpp>" PARENT_SCOPE)
  else(WIN32)
    set(test_path "${dir}:$ENV{PATH}" PARENT_SCOPE)
  endif(WIN32)
endfunction()

set_test_path("$<TARGET_FILE_DIR:broker>")

add_test(NAME cpp_container_example_test
  COMMAND ${env_py} -- "PATH=${test_path}" ${VALGRIND_ENV} ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v ContainerExampleTest)

set_test_path("$<TARGET_FILE_DIR:engine-broker>")

add_test(NAME cpp_engine_example_test
  COMMAND ${env_py} -- "PATH=${test_path}" ${VALGRIND_ENV} ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v ConnectionEngineExampleTest)
