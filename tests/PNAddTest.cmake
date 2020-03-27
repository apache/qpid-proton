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

# Helper function to define execution environment for ctest test targets

include(CMakeParseArguments)
include(CTest)

function(pn_add_test)
  set(options EXECUTABLE INTERPRETED UNWRAPPED IGNORE_ENVIRONMENT)
  set(oneValueArgs NAME COMMAND WORKING_DIRECTORY)
  set(multiValueArgs PREPEND_ENVIRONMENT APPEND_ENVIRONMENT)

  # Semicolon is CMake list separator and path separator on Windows; cmake_parse_arguments flattens nested lists!
  # Replace \; to /////, call cmake_parse_arguments, then replace back where it matters; ///// is unlikely to appear
  STRING(REPLACE \\\; ///// escaped_ARGN "${ARGN}")
  cmake_parse_arguments(pn_add_test "${options}" "${oneValueArgs}" "${multiValueArgs}" "${escaped_ARGN}")

  set(escaped_environment ${pn_add_test_PREPEND_ENVIRONMENT} ${TEST_ENV} ${pn_add_test_APPEND_ENVIRONMENT})
  STRING(REPLACE ///// \\\; environment "${escaped_environment}")

  if (pn_add_test_IGNORE_ENVIRONMENT)
    set (ignore_environment "--ignore_environment")
  else(pn_add_test_IGNORE_ENVIRONMENT)
    set (ignore_environment "")
  endif(pn_add_test_IGNORE_ENVIRONMENT)

  if (pn_add_test_UNWRAPPED)
    set (wrapper "")
  elseif(pn_add_test_INTERPRETED)
    set (wrapper "${TEST_WRAP_PREFIX_CMD}")
  elseif(pn_add_test_EXECUTABLE)
    set (wrapper "${TEST_EXE_PREFIX_CMD}")
  else()
    message(FATAL_ERROR "pn_add_test requires one of EXECUTABLE INTERPRETED UNWRAPPED")
  endif()

  add_test (
    NAME "${pn_add_test_NAME}"
    COMMAND ${PN_ENV_SCRIPT} ${ignore_environment} -- ${environment} ${wrapper} ${pn_add_test_COMMAND} ${pn_add_test_UNPARSED_ARGUMENTS}
    WORKING_DIRECTORY "${pn_add_test_WORKING_DIRECTORY}"
  )

  # TODO jdanek 2020-01-17: this could be used instead of env.py, it looks CMake 2.8.12 compatible
  #set_tests_properties("${pn_add_test_NAME}" PROPERTIES ENVIRONMENT "${environment}")
endfunction(pn_add_test)
