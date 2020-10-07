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


if (NOT PYTHON_EXECUTABLE)
  return()
endif()

if(WIN32)
  # NOTE: need to escape semicolons as cmake uses them as list separators.
  set(c_test_path "$<TARGET_FILE_DIR:c-broker>\;$<TARGET_FILE_DIR:qpid-proton-core>\;$<TARGET_FILE_DIR:qpid-proton-proactor>")
else()
  set(c_test_path "$<TARGET_FILE_DIR:c-broker>:$ENV{PATH}")
endif()

set(c_test_env
  "PATH=${c_test_path}"
  "PYTHONPATH=${Proton_SOURCE_DIR}/tests/py")

pn_add_test(
  UNWRAPPED
  NAME c-example-tests
  PREPEND_ENVIRONMENT "${c_test_env}"
  WORKING_DIRECTORY ${ProtonCExamples_SOURCE_DIR}
  COMMAND ${PYTHON_EXECUTABLE} testme -v)

if (BUILD_CPP)
  if(WIN32)
    # NOTE: need to escape semicolons as cmake uses them as list separators.
    set(cpp_test_path "$<TARGET_FILE_DIR:broker>\;$<TARGET_FILE_DIR:qpid-proton-core>\;$<TARGET_FILE_DIR:qpid-proton-cpp>")
  else()
    set(cpp_test_path "$<TARGET_FILE_DIR:broker>:$ENV{PATH}")
  endif()

  set(cpp_test_env
    "PATH=${cpp_test_path}"
    "PYTHONPATH=${Proton_SOURCE_DIR}/tests/py"
    "HAS_CPP11=$<$<BOOL:${HAS_ENOUGH_CPP11}>:1>")

  pn_add_test(
    UNWRAPPED
    NAME cpp-example-container
    PREPEND_ENVIRONMENT "${cpp_test_env}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${ProtonCppExamples_SOURCE_DIR}/testme -v ContainerExampleTest)

  if (NOT SSL_IMPL STREQUAL none)
    pn_add_test(
      UNWRAPPED
      NAME cpp-example-container-ssl
      PREPEND_ENVIRONMENT "${cpp_test_env}"
      WORKING_DIRECTORY ${ProtonCppExamples_SOURCE_DIR}
      COMMAND ${PYTHON_EXECUTABLE} testme -v ContainerExampleSSLTest)
  endif()
endif()
