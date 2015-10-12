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
  set(examples electron/receive electron/send electron/broker proton/broker)

  foreach(example ${examples})
    string(REPLACE / - target ${example})
    add_custom_target(go-example-${target} ALL
      COMMAND ${GO_BUILD} ${GO_EXAMPLE_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${example} ${CMAKE_CURRENT_SOURCE_DIR}/${example}.go
      DEPENDS go-packages qpid-proton)
  endforeach()
  add_custom_target(go-example-test ALL
      COMMAND ${GO_TEST} -c ${GO_EXAMPLE_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/example_test ${CMAKE_CURRENT_SOURCE_DIR}/example_test.go
      DEPENDS go-packages qpid-proton)

  add_test(
    NAME go_example_electron_test
    COMMAND ${GO_ENV} ${CMAKE_CURRENT_BINARY_DIR}/example_test -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker broker)


  add_test(
    NAME go_example_proton_test
    COMMAND ${GO_ENV} ${CMAKE_CURRENT_BINARY_DIR}/example_test -dir ${CMAKE_CURRENT_BINARY_DIR}/electron -broker ../proton/broker)

  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES ${examples})
endif()
