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
execute_process(COMMAND ${GO_EXE} version OUTPUT_VARIABLE go_ver OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "go[0-9]+.[0-9]+[0-9]*" GO_VERSION "${go_ver}")
string(SUBSTRING ${GO_VERSION} 2 -1 GOLESS_VERSION)
message(STATUS "Found Go: ${GO_EXE} (${go_ver}) (${GOLESS_VERSION}).")

if (GOLESS_VERSION VERSION_LESS 1.11)
  set(BUILD_GO "OFF")
  message(STATUS "Go: ${GO_EXE} (${GOLESS_VERSION}) version to low. At least 1.11 required.")
endif()

if (BUILD_GO)
  # NOTE: go test -race flag is not included by default, it causes problems on several platforms:
  # - ubuntu up to trust: link errors
  # - ubuntu from xenial: requires extra package golang-race-detector-runtime
  # - fedora with gccgo: complains about "import cycles"
  # (Works well on fedora with original go)
  # Enable manually with -DGO_TEST_FLAGS="-v -race"

  set(GO_BUILD_FLAGS "" CACHE STRING "Flags for 'go build'")
  set(GO_VET_FLAGS "-v" CACHE STRING "Flags for 'go test'")
  set(GO_TEST_FLAGS "-v" CACHE STRING "Flags for 'go test'")

  # Flags that differ for golang go and gcc go.
  if (go_ver MATCHES "gccgo")
    set(GO_RPATH_FLAGS -gccgoflags "-Wl,-rpath=${PN_C_LIBRARY_DIR}")
  else()
    set(GO_RPATH_FLAGS -ldflags "-r ${PN_C_LIBRARY_DIR}")
  endif()

  separate_arguments(GO_BUILD_FLAGS)
  separate_arguments(GO_TEST_FLAGS)

  # Create a Go tree in the binary directory, link pkg to the source directory, link go.mod file
  set(GOPATH ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(go-pkg-link ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/pkg ${GOPATH}/pkg)
  add_custom_target(go-mod-link ALL
          COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../go.mod ${GOPATH}/../go.mod)

  # Following are CACHE INTERNAL so examples/CMakeLists.txt can see them.
  set(GO_ENV
    "GOPATH=${GOPATH}"
    "CGO_CFLAGS=-I${PN_C_INCLUDE_DIR}"
    "CGO_LDFLAGS=-L${PN_C_LIBRARY_DIR}"
    "PN_INTEROP_DIR=${CMAKE_SOURCE_DIR}/tests/interop"
    "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
    CACHE INTERNAL "Run a command with Go environment variables")

  set(GO ${PN_ENV_SCRIPT} -- ${GO_ENV} ${GO_EXE} CACHE INTERNAL "Run go with environment set")

  set(GO_BUILD ${GO} build ${GO_BUILD_FLAGS} ${GO_RPATH_FLAGS} CACHE INTERNAL "Run go build")
  set(GO_INSTALL ${GO} install ${GO_BUILD_FLAGS} CACHE INTERNAL "Run go install" )
  set(GO_TEST ${GO} test ${GO_BUILD_FLAGS} ${GO_RPATH_FLAGS} ${GO_TEST_FLAGS} CACHE INTERNAL "Run go test")

  # The go build tools handle dependency checks and incremental builds better than
  # CMake so just run them every time, they do nothing if nothing needs to be
  # done.
  add_custom_target(go-build ALL
    COMMAND ${GO_INSTALL} ${CMAKE_CURRENT_BINARY_DIR}/pkg/...
    DEPENDS qpid-proton-core go-pkg-link go-mod-link
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_test(
    NAME go-test
    COMMAND ${GO_TEST} ${CMAKE_CURRENT_BINARY_DIR}/pkg/...
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  # Clean up go output directories.
  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES ${GOPATH}/pkg ${GOPATH}/bin)

  if (BUILD_EXAMPLES)
    add_subdirectory(examples)
  endif (BUILD_EXAMPLES)

  # Install go sources.
  set (GO_INSTALL_DIR ${SHARE_INSTALL_DIR}/gocode/pkg CACHE PATH "Installation directory for Go code")
  mark_as_advanced (GO_INSTALL_DIR)

  install(DIRECTORY pkg DESTINATION ${GO_INSTALL_DIR} COMPONENT Go)
  install(DIRECTORY examples/
    DESTINATION "${PROTON_SHARE}/examples/go"
    COMPONENT Go
    PATTERN "CMakeLists.txt" EXCLUDE)
endif()
