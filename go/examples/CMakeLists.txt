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
    string(REPLACE / _ target ${example})
    set(target "go_example_${target}")
    set(output ${CMAKE_CURRENT_BINARY_DIR}/${example})
    # Always run go_build, it will do nothing if there is nothing to do.
    # Otherwise it's too hard to get the dependencies right.
    add_custom_target(${target} ALL
      COMMAND ${GO_BUILD} ${GO_EXAMPLE_FLAGS} -o ${output} ${CMAKE_CURRENT_SOURCE_DIR}/${example}.go
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS go-build)
    list(APPEND example_targets ${target})
  endforeach()

  # Build test driver exe
  set(test_exe ${CMAKE_CURRENT_BINARY_DIR}/example_test)
  add_custom_target(go_example_test ALL
    COMMAND ${GO_TEST} -c -o ${test_exe} ${CMAKE_CURRENT_SOURCE_DIR}/example_test.go
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  pn_add_test(
    UNWRAPPED
    NAME go-example-electron
    PREPEND_ENVIRONMENT ${GO_ENV}
    COMMAND ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker broker)

  pn_add_test(
    UNWRAPPED
    NAME go-example-proton
    PREPEND_ENVIRONMENT ${GO_ENV}
    COMMAND ${test_exe} -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker ../proton/broker)
  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES ${examples})
endif()
