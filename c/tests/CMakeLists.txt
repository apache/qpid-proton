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
  set(test_env "PATH=$<TARGET_FILE_DIR:qpid-proton-core>" ${TEST_ENV})
else()
  set(test_env ${TEST_ENV})
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
      COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARNING_FLAGS}")
    add_test(NAME ${exe} COMMAND ${PN_ENV_SCRIPT} -- ${test_env} ${TEST_EXE_PREFIX_CMD} $<TARGET_FILE:${exe}>)
    set_tests_properties(${exe} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endmacro()

  ## Tests that depend only on qpid-proton-core
  add_c_test(c-core-test
    pn_test_test.cpp
    object_test.cpp
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

    add_c_test(c-ssl-proactor-test pn_test_proactor.cpp ssl_proactor_test.cpp)
    target_link_libraries(c-ssl-proactor-test qpid-proton-core qpid-proton-proactor ${PLATFORM_LIBS})

    # Thread race test.
    #
    # TODO aconway 2018-11-14: enable by default when races and xcode
    # problems are cleared up
    option(THREADERCISER "Run the threaderciser concurrency tests" OFF)
    if (THREADERCISER)
      add_executable(c-threaderciser threaderciser.c)
      set_target_properties(c-threaderciser PROPERTIES COMPILE_FLAGS "${CMAKE_C_FLAGS} ${C_WARNING_FLAGS}")
      add_test(NAME c-threaderciser COMMAND ${PN_ENV_SCRIPT} -- ${test_env} ${TEST_EXE_PREFIX_CMD} $<TARGET_FILE:c-threaderciser>)
      set_tests_properties(c-threaderciser PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
      target_link_libraries (c-threaderciser qpid-proton-proactor)
      find_library(Pthread_LIBRARY pthread)
      if (Pthread_LIBRARY)
        target_link_libraries (c-threaderciser ${Pthread_LIBRARY})
      endif()
    endif()

    if(WIN32)
      set(path "$<TARGET_FILE_DIR:c-broker>\\;$<TARGET_FILE_DIR:qpid-proton>")
    else(WIN32)
      set(path "$<TARGET_FILE_DIR:c-broker>:$ENV{PATH}")
    endif(WIN32)

    set(pypath "${CMAKE_SOURCE_DIR}/tests/py")

    # unset TEST_EXE_PREFIX as valgrind does not run succesfully when fds are limited
    add_test(
        NAME c-fdlimit-tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${PN_ENV_SCRIPT} -- "PATH=${path}" "PYTHONPATH=${pypath}" ${test_env} "TEST_EXE_PREFIX=" ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/fdlimit.py)
  endif(HAS_PROACTOR)
else (CMAKE_CXX_COMPILER)
  message(WARNING "No C++ compiler, some C library tests were not built")
endif (CMAKE_CXX_COMPILER)

# fuzz tests: tests/fuzz
if (ENABLE_FUZZ_TESTING)
  add_subdirectory(fuzz)
endif (ENABLE_FUZZ_TESTING)
