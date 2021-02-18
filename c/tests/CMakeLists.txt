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

configure_file(test_config.h.in test_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/tests/include)
if (WIN32)
  set(test_env "PATH=$<TARGET_FILE_DIR:qpid-proton-core>")
else()
  set(test_env "")
  set(platform_test_src ssl_test.cpp)
endif()

# NOTE: C library tests are written in C++ using the Catch2 framework.
# The tests are more concise than with a native C framework and we can
# use the same framework for C++ tests.
# See https://github.com/catchorg/Catch2/blob/Catch1.x/docs/tutorial.md for more.
# We will use the Catch2 1.x branch for as long as we must support C++03
#
# ${CMAKE_SOURCE_DIR}/include/tests/catch_extra.cpp has some general extensions to catch.
# ./pn_test.hpp contains proton-C specific code to help with testing.
#
# Tests for each library are combined in a single executable.

if (CMAKE_CXX_COMPILER)

  add_library(test_main OBJECT test_main.cpp)

  macro(add_c_test exe)
    add_executable(${exe} $<TARGET_OBJECTS:test_main> pn_test.cpp ${ARGN})
    set_target_properties(${exe} PROPERTIES
      COMPILE_FLAGS "${CXX_STANDARD} ${CMAKE_CXX_FLAGS} ${CXX_WARNING_FLAGS} ${LTO}"
      LINK_FLAGS "${CATCH_UNDEFINED} ${LINK_LTO}")
    pn_add_test(
      EXECUTABLE
      NAME ${exe}
      PREPEND_ENVIRONMENT ${test_env}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND $<TARGET_FILE:${exe}>)
  endmacro()

  ## Tests that depend only on qpid-proton-core
  add_c_test(c-core-test
    pn_test_test.cpp
    object_test.cpp
    event_test.cpp
    message_test.cpp
    condition_test.cpp
    connection_driver_test.cpp
    data_test.cpp
    engine_test.cpp
    refcount_test.cpp
    ${platform_test_src})

  target_link_libraries(c-core-test qpid-proton-core ${PLATFORM_LIBS})

  ## Tests for the deprecated "extra" part of the qpid-proton library.
  add_c_test(c-extra-test url_test.cpp)
  target_link_libraries(c-extra-test qpid-proton ${PLATFORM_LIBS})

  if(HAS_PROACTOR)
    # Tests for qpid-proton-proactor
    add_c_test(c-proactor-test pn_test_proactor.cpp proactor_test.cpp)
    target_link_libraries(c-proactor-test qpid-proton-core qpid-proton-proactor ${PLATFORM_LIBS})

    add_c_test(c-raw-connection-test raw_connection_test.cpp $<TARGET_OBJECTS:qpid-proton-proactor-objects>)
    target_link_libraries(c-raw-connection-test qpid-proton-core ${PLATFORM_LIBS} ${PROACTOR_LIBS})

    add_c_test(c-ssl-proactor-test pn_test_proactor.cpp ssl_proactor_test.cpp)
    target_link_libraries(c-ssl-proactor-test qpid-proton-core qpid-proton-proactor ${PLATFORM_LIBS})

    # Thread race test.
    # Temporarily disable threaderciser tests by default. Re-enabled in CI configs as appropriate.
    set(DEFAULT_THREADERCISER OFF)
    option(THREADERCISER "Run the threaderciser concurrency tests" ${DEFAULT_THREADERCISER})
    if (THREADERCISER)
      add_executable(c-threaderciser threaderciser.c)
      set_target_properties(c-threaderciser
        PROPERTIES COMPILE_FLAGS "${C_STANDARD_FLAGS} ${COMPILE_WARNING_FLAGS}")
      target_link_libraries (c-threaderciser qpid-proton-proactor Threads::Threads)
      pn_add_test(
        EXECUTABLE
        NAME c-threaderciser
        PREPEND_ENVIRONMENT ${test_env}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:c-threaderciser>)
    endif()

    if(WIN32)
      set(path "$<TARGET_FILE_DIR:c-broker>\\;$<TARGET_FILE_DIR:qpid-proton>")
    else(WIN32)
      set(path "$<TARGET_FILE_DIR:c-broker>:$ENV{PATH}")
    endif(WIN32)

    set(pypath "${CMAKE_SOURCE_DIR}/tests/py")

    # unset TEST_EXE_PREFIX as valgrind does not run successfully when fds are limited
    pn_add_test(
      UNWRAPPED
      NAME c-fdlimit-tests
      PREPEND_ENVIRONMENT "PATH=${path}" "PYTHONPATH=${pypath}" "${test_env}"
      APPEND_ENVIRONMENT "TEST_EXE_PREFIX="
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/fdlimit.py)
  endif(HAS_PROACTOR)
else (CMAKE_CXX_COMPILER)
  message(WARNING "No C++ compiler, some C library tests were not built")
endif (CMAKE_CXX_COMPILER)

# fuzz tests: tests/fuzz
if (ENABLE_FUZZ_TESTING)
  add_subdirectory(fuzz)
endif (ENABLE_FUZZ_TESTING)
