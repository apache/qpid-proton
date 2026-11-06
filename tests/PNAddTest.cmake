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

# Helper function to define execution environment for ctest test targets.

include(CTest)

# pn_add_test(
#   EXECUTABLE | INTERPRETED | UNWRAPPED    # required, see below
#   NAME <name>
#   COMMAND <command>
#   [WORKING_DIRECTORY <dir>]
#   [ENVIRONMENT <NAME=VALUE>...]           # set for this test
#   [UNSET_ENVIRONMENT <NAME>...]           # removed for this test
#   [PATH_PREPEND <dir>...]                 # prepended to PATH
#   [PYTHONPATH_PREPEND <dir>...]           # prepended to PYTHONPATH
#   [RUBYLIB_PREPEND <dir>...]              # prepended to RUBYLIB
#   [ENV_MODIFICATION <NAME=OP:VALUE>...]   # raw ENVIRONMENT_MODIFICATION entries
#   [<extra arguments for COMMAND>...]
# )
#
# One of these is required, to say how the test is wrapped for runtime checking:
#   EXECUTABLE  - a compiled program, run under TEST_EXE_PREFIX
#   INTERPRETED - an interpreter (python, ruby) loading proton as an extension,
#                 run under TEST_WRAP_PREFIX
#   UNWRAPPED   - run as-is, no runtime check wrapper
#
# Environment precedence, lowest to highest:
#   1. the environment ctest itself was run with
#   2. ENVIRONMENT entries given here
#   3. TEST_ENV, so an explicit -DTEST_ENV=... on the cmake command line always wins
#   4. the *_PREPEND, UNSET_ENVIRONMENT and ENV_MODIFICATION entries, which CTest
#      applies after everything above
#
# The *_PREPEND arguments take individual directories, not a pre-joined list,
# and keep the order they are given in: PATH_PREPEND a b c puts "a:b:c" in
# front of PATH. CTest applies them when the test runs, using the native path
# separator, so there is no need to join them by hand or to special case
# Windows, and they are prepended to the real PATH rather than to whatever
# PATH happened to be set when cmake ran.

# Add ENVIRONMENT_MODIFICATION entries to <out> prepending the directories in
# the list variable <dirs_var> to the search path <name>. CTest applies the
# entries in order and each one goes to the front, so emit them last to first
# to end up with the caller's ordering.
macro(_pn_path_prepend out name dirs_var)
  set(_pn_dirs ${${dirs_var}})
  if (_pn_dirs)
    list(REVERSE _pn_dirs)
    foreach(_pn_dir IN LISTS _pn_dirs)
      list(APPEND ${out} "${name}=path_list_prepend:${_pn_dir}")
    endforeach()
  endif()
endmacro()

function(pn_add_test)
  set(options EXECUTABLE INTERPRETED UNWRAPPED)
  set(oneValueArgs NAME COMMAND WORKING_DIRECTORY)
  set(multiValueArgs
    ENVIRONMENT UNSET_ENVIRONMENT ENV_MODIFICATION
    PATH_PREPEND PYTHONPATH_PREPEND RUBYLIB_PREPEND)

  # PARSE_ARGV keeps each argument intact, so a value containing a literal
  # semicolon (e.g. a PKCS#11 URI) survives without any escaping games.
  cmake_parse_arguments(PARSE_ARGV 0 arg
    "${options}" "${oneValueArgs}" "${multiValueArgs}")

  if (arg_UNWRAPPED)
    set(wrapper "")
  elseif (arg_INTERPRETED)
    set(wrapper "${TEST_WRAP_PREFIX_CMD}")
  elseif (arg_EXECUTABLE)
    set(wrapper "${TEST_EXE_PREFIX_CMD}")
  else()
    message(FATAL_ERROR "pn_add_test requires one of EXECUTABLE INTERPRETED UNWRAPPED")
  endif()

  add_test(
    NAME "${arg_NAME}"
    COMMAND ${wrapper} ${arg_COMMAND} ${arg_UNPARSED_ARGUMENTS}
    WORKING_DIRECTORY "${arg_WORKING_DIRECTORY}"
  )

  set(environment ${arg_ENVIRONMENT} ${TEST_ENV})

  set(modification ${arg_ENV_MODIFICATION})
  _pn_path_prepend(modification PATH arg_PATH_PREPEND)
  _pn_path_prepend(modification PYTHONPATH arg_PYTHONPATH_PREPEND)
  _pn_path_prepend(modification RUBYLIB arg_RUBYLIB_PREPEND)
  foreach(name IN LISTS arg_UNSET_ENVIRONMENT)
    list(APPEND modification "${name}=unset:")
  endforeach()

  set_tests_properties("${arg_NAME}" PROPERTIES
    ENVIRONMENT "${environment}"
    ENVIRONMENT_MODIFICATION "${modification}")
endfunction(pn_add_test)
