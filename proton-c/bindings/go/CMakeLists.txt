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

# Go version
execute_process(COMMAND ${GO_EXE} version OUTPUT_VARIABLE go_out)
message(STATUS "Found Go: ${go_out}")

set(GO_BUILD_FLAGS "" CACHE STRING "Flags for 'go build'")

# Flags that differ for golang go and gcc go.
if (go_out MATCHES "gccgo")
  # TODO aconway 2015-10-08: import cycles with -race under gccgo, investigate.
  set(GO_TEST_FLAGS "-v" CACHE STRING "Flags for 'go test'")
  set(GO_RPATH_FLAGS -gccgoflags "-Wl,-rpath=${CMAKE_BINARY_DIR}/proton-c")
else()
  set(GO_TEST_FLAGS "-v -race" CACHE STRING "Flags for 'go test'")
  set(GO_RPATH_FLAGS -ldflags "-r ${CMAKE_BINARY_DIR}/proton-c")
endif()

separate_arguments(GO_BUILD_FLAGS)
separate_arguments(GO_TEST_FLAGS)

# Following are CACHE INTERNAL so examples/CMakeLists.txt can see them.
set(GO_ENV ${env_py} --
  "GOPATH=${CMAKE_CURRENT_SOURCE_DIR}"
  "CGO_CFLAGS=-I${CMAKE_SOURCE_DIR}/proton-c/include"
  "CGO_LDFLAGS=-L${CMAKE_BINARY_DIR}/proton-c"
  CACHE INTERNAL "Run a command with Go environment variables")

set(GO ${GO_ENV} ${GO_EXE} CACHE INTERNAL "Run go with environment set")

set(GO_BUILD ${GO} build ${GO_BUILD_FLAGS} ${GO_RPATH_FLAGS} CACHE INTERNAL "Run go build")
set(GO_INSTALL ${GO} install ${GO_BUILD_FLAGS} CACHE INTERNAL "Run go install")
set(GO_TEST ${GO} test ${GO_BUILD_FLAGS} ${GO_RPATH_FLAGS} ${GO_TEST_FLAGS} CACHE INTERNAL "Run go test")

# Install packages in the source tree, go tools aren't friendly otherwise.
# All build output goes in git-ignored pkg or bin subdirectories.
set(q "qpid.apache.org")
set(packages ${q}/amqp ${q}/internal ${q}/proton ${q}/electron)
add_custom_target(go-packages ALL
  COMMAND ${GO_INSTALL} ${packages}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS qpid-proton)

add_test(
  NAME go_test_packages
  COMMAND ${GO_TEST} ${packages}
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

list(APPEND ADDITIONAL_MAKE_CLEAN_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/pkg
  ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set (GO_INSTALL_DIR ${SHARE_INSTALL_DIR}/gocode/src CACHE PATH "Installation directory for Go code")
mark_as_advanced (GO_INSTALL_DIR)
install(DIRECTORY src/qpid.apache.org DESTINATION ${GO_INSTALL_DIR} COMPONENT Go)
