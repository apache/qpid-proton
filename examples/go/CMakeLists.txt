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

if(BUILD_GO)

  set(examples electron/broker electron/receive electron/send proton/broker)
  file(GLOB_RECURSE example_source FOLLOW_SYMLINKS ${CMAKE_CURRENT_SOURCE_DIR}/*.go)

  # Build example exes
  foreach(example ${examples})
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${example}.go)
    set(target ${CMAKE_CURRENT_BINARY_DIR}/${example})
    add_custom_command(
      OUTPUT ${target}
      COMMAND ${GO_BUILD} ${GO_EXAMPLE_FLAGS} -o ${target} ${source}
      DEPENDS  ${example_source} ${GO_TARGETS}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    list(APPEND example_exes ${target})
  endforeach()

  # Build test driver exe
  set(test_exe ${CMAKE_CURRENT_BINARY_DIR}/example_test)
  add_custom_command(
    OUTPUT ${test_exe}
    DEPENDS ${example_exes} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.go
    COMMAND ${GO_TEST} -c -o ${test_exe} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.go
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_custom_target(go-test-exe ALL DEPENDS ${test_exe})

  add_test(
    NAME go_example_electron_test
    COMMAND ${GO_ENV} ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker broker)

  add_test(
    NAME go_example_proton_test
    COMMAND ${GO_ENV} ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker ../proton/broker)

  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES ${examples})
endif()
