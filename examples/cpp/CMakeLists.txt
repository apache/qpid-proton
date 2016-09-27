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

# Add a test with the correct environment to find test executables and valgrind.
macro(add_cpp_test name)
  if(WIN32)
    set(test_path "$<TARGET_FILE_DIR:broker>;$<TARGET_FILE_DIR:qpid-proton>;$<TARGET_FILE_DIR:qpid-proton-cpp>")
  else(WIN32)
    set(test_path "$<TARGET_FILE_DIR:broker>:$ENV{PATH}")
  endif(WIN32)
  set(run_env ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/proton-c/env.py)
  add_test(NAME ${name} COMMAND ${run_env} "PATH=${test_path}" ${VALGRIND_ENV} -- ${ARGN})
endmacro()

# Single-threaded examples that work on C++03
foreach(example
    broker
    helloworld
    helloworld_direct
    simple_recv
    simple_send
    scheduled_send_03
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
    service_bus
    encode_decode)
  add_executable(${example} ${example}.cpp)
endforeach()

if(HAS_CPP11)
  # Single-threaded examples that require C++11
  foreach(example
      scheduled_send)
    add_executable(${example} ${example}.cpp)
  endforeach()

  # Linux-only multi-threaded examples (TODO make these portable)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(container_src mt/epoll_container.cpp)
    foreach(example
        broker)
      add_executable(mt_${example} mt/${example}.cpp ${container_src})
      target_link_libraries(mt_${example} pthread)
    endforeach()
    add_cpp_test(cpp-example-mt ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v MtBrokerTest)
  endif()
endif()

add_cpp_test(cpp-example-container ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v ContainerExampleTest)



