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

include_directories ("${CMAKE_SOURCE_DIR}/proton-c/bindings/cpp/include")

foreach(example broker helloworld helloworld_blocking helloworld_direct simple_recv simple_send)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} qpid-proton-cpp)
endforeach()

add_test(
  NAME cpp_example_test
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.py -v
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
