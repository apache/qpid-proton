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

add_definitions(${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS})

option(FUZZ_REGRESSION_TESTS "Run fuzz tests with regression test driver" ON)
option(FUZZ_LONG_TESTS "Run fuzz tests that take a long time" OFF)

if (FUZZ_REGRESSION_TESTS)
  set(FUZZING_LIBRARY StandaloneFuzzTargetMain)
else ()
  set(FUZZING_LIBRARY FuzzingEngine)
endif ()

add_library (StandaloneFuzzTargetMain STATIC StandaloneFuzzTargetMain.c StandaloneFuzzTargetInit.c)

macro (pn_add_fuzz_test test)
  add_executable (${test} ${ARGN})
  target_link_libraries (${test} qpid-proton-core ${FUZZING_LIBRARY})

  if (FUZZ_REGRESSION_TESTS)
    # StandaloneFuzzTargetMain cannot walk directory trees
    file(GLOB_RECURSE files ${CMAKE_CURRENT_SOURCE_DIR}/${test}/*)
    add_test (NAME ${test} COMMAND ${TEST_EXE_PREFIX_CMD} $<TARGET_FILE:${test}> ${files})
  else ()
    add_test (NAME ${test} COMMAND $<TARGET_FILE:${test}> -runs=1 ${CMAKE_CURRENT_SOURCE_DIR}/${test}>)
  endif ()
endmacro(pn_add_fuzz_test)

# Fuzz tests at the User API level
pn_add_fuzz_test (fuzz-connection-driver fuzz-connection-driver.c)
pn_add_fuzz_test (fuzz-message-decode fuzz-message-decode.c)

# pn_url_parse is not in proton core and is only used by messenger so compile specially
pn_add_fuzz_test (fuzz-url fuzz-url.c ${PN_C_SOURCE_DIR}/extra/url.c  ${PN_C_SOURCE_DIR}/core/util.c)

# This regression test can take a very long time so don't run by default
if(HAS_PROACTOR AND FUZZ_LONG_TESTS)
  pn_add_fuzz_test (fuzz-proactor-receive fuzz-proactor-receive.c)
  target_link_libraries(fuzz-proactor-receive qpid-proton-proactor)
endif()

# Fuzz tests of internals

# pni_sniff_header is internal so it has to be compiled specially
pn_add_fuzz_test (fuzz-sniff-header fuzz-sniff-header.c ${PN_C_SOURCE_DIR}/core/autodetect.c)
