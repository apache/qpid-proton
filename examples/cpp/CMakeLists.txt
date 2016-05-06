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
link_libraries(${ProtonCpp_LIBRARIES})
add_definitions(${CXX_WARNING_FLAGS})

# Single-threaded examples.
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
    connection_options
    queue_browser
    selected_recv
    flow_control
    ssl
    ssl_client_cert
    encode_decode)
  add_executable(${example} ${example}.cpp)
endforeach()

# Python test runner
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

# TODO aconway 2016-04-26: need portable MT and IO examples.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HAS_CPP11)
  set(container_src mt/epoll_container.cpp)
  foreach(example broker)       # More coming
    add_executable(mt_${example} mt/${example}.cpp ${container_src})
    target_link_libraries(mt_${example} pthread)
  endforeach()

  add_test(NAME cpp_mt_example_test
    COMMAND ${env_py} -- "PATH=${test_path}" ${VALGRIND_ENV} ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v MtBrokerTest)
endif()



