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

# Configuration for code analysis tools: runtime checking and coverage.

# Any test that needs runtime checks should use TEST_EXE_PREFIX and TEST_ENV.
# Normally they are set as a result of the RUNTIME_CHECK value,
# but can be set directly for unsupported tools or unusual flags
# e.g. -DTEST_EXE_PREFIX=rr or -DTEST_EXE_PREFIX="valgrind --tool=massif"

set(TEST_EXE_PREFIX "" CACHE STRING "Prefix for test executable command line")
set(TEST_WRAP_PREFIX "" CACHE STRING "Prefix for interpreter tests (e.g. python, ruby) that load proton as an extension")
set(TEST_ENV "" CACHE STRING "Extra environment for tests: name1=value1;name2=value2")
mark_as_advanced(TEST_EXE_PREFIX TEST_WRAP_PREFIX TEST_ENV)

# Check for valgrind
find_program(VALGRIND_EXECUTABLE valgrind DOC "location of valgrind program")
set(VALGRIND_SUPPRESSIONS "${CMAKE_SOURCE_DIR}/tests/valgrind.supp" CACHE STRING "Suppressions file for valgrind")
set(VALGRIND_COMMON_ARGS "--error-exitcode=42 --quiet --suppressions=${VALGRIND_SUPPRESSIONS}")
mark_as_advanced(VALGRIND_EXECUTABLE VALGRIND_SUPPRESSIONS VALGRIND_COMMON_ARGS)

# Check for compiler sanitizers
if((CMAKE_C_COMPILER_ID MATCHES "GNU"
      AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.8)
    OR (CMAKE_C_COMPILER_ID MATCHES "Clang"
      AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.1))
  set(HAS_SANITIZERS TRUE)
endif()

# Valid values for RUNTIME_CHECK
set(runtime_checks OFF asan tsan memcheck helgrind)

# Set the default
if(NOT CMAKE_BUILD_TYPE MATCHES "Coverage" AND VALGRIND_EXECUTABLE)
  set(RUNTIME_CHECK_DEFAULT "memcheck")
endif()

# Deprecated options to enable runtime checks
macro(deprecated_enable_check old new doc)
  option(${old} ${doc} OFF)
  if (${old})
    message("WARNING: option ${old} is deprecated, use RUNTIME_CHECK=${new} instead")
    set(RUNTIME_CHECK_DEFAULT ${new})
  endif()
endmacro()
deprecated_enable_check(ENABLE_VALGRIND memcheck "Use valgrind to detect run-time problems")
deprecated_enable_check(ENABLE_SANITIZERS asan "Compile with memory sanitizers (asan, ubsan)")
deprecated_enable_check(ENABLE_TSAN tsan "Compile with thread sanitizer (tsan)")

set(RUNTIME_CHECK ${RUNTIME_CHECK_DEFAULT} CACHE string "Enable runtime checks. Valid values: ${runtime_checks}")

if(CMAKE_BUILD_TYPE MATCHES "Coverage" AND RUNTIME_CHECK)
  message(FATAL_ERROR "Cannot set RUNTIME_CHECK with CMAKE_BUILD_TYPE=Coverage")
endif()

macro(assert_has_sanitizers)
  if(NOT HAS_SANITIZERS)
    message(FATAL_ERROR "compiler sanitizers are not available")
  endif()
endmacro()

macro(assert_has_valgrind)
  if(NOT VALGRIND_EXECUTABLE)
    message(FATAL_ERROR "valgrind is not available")
  endif()
endmacro()

if(RUNTIME_CHECK STREQUAL "memcheck")
  assert_has_valgrind()
  message(STATUS "Runtime memory checker: valgrind memcheck")
  set(TEST_EXE_PREFIX "${VALGRIND_EXECUTABLE} --tool=memcheck --leak-check=full ${VALGRIND_COMMON_ARGS}")
  # TODO aconway 2018-09-06: NO TEST_WRAP_PREFIX, need --trace-children + many suppressions

elseif(RUNTIME_CHECK STREQUAL "helgrind")
  assert_has_valgrind()
  message(STATUS "Runtime race checker: valgrind helgrind")
  set(TEST_EXE_PREFIX "${VALGRIND_EXECUTABLE} --tool=helgrind ${VALGRIND_COMMON_ARGS}")
  # TODO aconway 2018-09-06: NO TEST_WRAP_PREFIX, need --trace-children + many suppressions

elseif(RUNTIME_CHECK STREQUAL "asan")
  assert_has_sanitizers()
  message(STATUS "Runtime memory checker: gcc/clang memory sanitizers")
  set(SANITIZE_FLAGS "-g -fno-omit-frame-pointer -fsanitize=address,undefined")
  set(TEST_WRAP_PREFIX "${CMAKE_SOURCE_DIR}/tests/preload_asan.sh $<TARGET_FILE:qpid-proton-core>")

elseif(RUNTIME_CHECK STREQUAL "tsan")
  assert_has_sanitizers()
  message(STATUS "Runtime race checker: gcc/clang thread sanitizer")
  set(SANITIZE_FLAGS "-g -fno-omit-frame-pointer -fsanitize=thread")

elseif(RUNTIME_CHECK)
  message(FATAL_ERROR "'RUNTIME_CHECK=${RUNTIME_CHECK}' is invalid, valid values: ${runtime_checks}")
endif()

if(SANITIZE_FLAGS)
  set(ENABLE_UNDEFINED_ERROR OFF CACHE BOOL "Disabled for sanitizers" FORCE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
endif()

if(TEST_EXE_PREFIX)
  # Add TEST_EXE_PREFIX to TEST_ENV so test runner scripts can use it.
  list(APPEND TEST_ENV "TEST_EXE_PREFIX=${TEST_EXE_PREFIX}")
  # Make a CMake-list form of TEST_EXE_PREFIX for add_test() commands
  separate_arguments(TEST_EXE_PREFIX_CMD UNIX_COMMAND "${TEST_EXE_PREFIX}")
endif()
separate_arguments(TEST_WRAP_PREFIX_CMD UNIX_COMMAND "${TEST_WRAP_PREFIX}")
