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


if (NOT Python_EXECUTABLE)
  return()
endif()

set(c_test_path $<TARGET_FILE_DIR:c-broker>)
if(WIN32)
  # Windows looks for DLLs on PATH
  list(APPEND c_test_path
    $<TARGET_FILE_DIR:qpid-proton-core> $<TARGET_FILE_DIR:qpid-proton-proactor>)
endif()

pn_add_test(
  UNWRAPPED
  NAME c-example-tests
  ENVIRONMENT "SSL_CERT_DIR=${TEST_CERT_DIR}"
  PATH_PREPEND ${c_test_path}
  PYTHONPATH_PREPEND "${Proton_SOURCE_DIR}/tests/py"
  WORKING_DIRECTORY ${ProtonCExamples_SOURCE_DIR}
  COMMAND ${Python_EXECUTABLE} testme -v)

if (BUILD_CPP)
  set(cpp_test_path $<TARGET_FILE_DIR:broker>)
  if(WIN32)
    # Windows looks for DLLs on PATH
    list(APPEND cpp_test_path
      $<TARGET_FILE_DIR:qpid-proton-core> $<TARGET_FILE_DIR:qpid-proton-cpp>)
  endif()

  set(cpp_test_env "TEST_CERT_DIR=${TEST_CERT_DIR}")

  pn_add_test(
    UNWRAPPED
    NAME cpp-example-container
    ENVIRONMENT ${cpp_test_env}
    PATH_PREPEND ${cpp_test_path}
    PYTHONPATH_PREPEND "${Proton_SOURCE_DIR}/tests/py"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${Python_EXECUTABLE} ${ProtonCppExamples_SOURCE_DIR}/testme -v ContainerExampleTest)

  if (NOT SSL_IMPL STREQUAL none)
    pn_add_test(
      UNWRAPPED
      NAME cpp-example-container-ssl
      ENVIRONMENT ${cpp_test_env}
      PATH_PREPEND ${cpp_test_path}
      PYTHONPATH_PREPEND "${Proton_SOURCE_DIR}/tests/py"
      WORKING_DIRECTORY ${ProtonCppExamples_SOURCE_DIR}
      COMMAND ${Python_EXECUTABLE} testme -v ContainerExampleSSLTest)
  endif()
endif()
