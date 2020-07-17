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

add_definitions(${C_STANDARD_FLAGS} ${COMPILE_WARNING_FLAGS})

option(FUZZ_REGRESSION_TESTS "Run fuzz tests with regression test driver" ON)
option(FUZZ_LONG_TESTS "Run fuzz tests that take a long time" OFF)

# prefer static lib for the fuzzer, if available
if (BUILD_STATIC_LIBS)
  set(FUZZING_QPID_PROTON_CORE_LIBRARY qpid-proton-core-static)
  set(FUZZING_QPID_PROTON_PROACTOR_LIBRARY qpid-proton-proactor-static)
else()
  set(FUZZING_QPID_PROTON_CORE_LIBRARY qpid-proton-core)
  set(FUZZING_QPID_PROTON_PROACTOR_LIBRARY qpid-proton-proactor)
endif()

if (FUZZ_REGRESSION_TESTS)
  set(FUZZING_LIBRARY StandaloneFuzzTargetMain)
else ()
  set(FUZZING_LIBRARY FuzzingEngine)
endif ()

add_library (StandaloneFuzzTargetMain STATIC StandaloneFuzzTargetMain.c StandaloneFuzzTargetInit.c)

macro (pn_add_fuzz_test test)
  add_executable (${test} ${ARGN})
  target_link_libraries (${test} ${FUZZING_LIBRARY})
  # -lFuzzingEngine is a C++ library, which needs c++ std lib
  set_target_properties(${test} PROPERTIES LINKER_LANGUAGE CXX)

  list(APPEND fuzz_test_src ${ARGN})

  if (FUZZ_REGRESSION_TESTS)
    # StandaloneFuzzTargetMain cannot walk directory trees
    file(GLOB_RECURSE files ${CMAKE_CURRENT_SOURCE_DIR}/${test}/*)
    unset(file_lines)
    foreach(f IN LISTS files)
      set(file_lines "${file_lines}${f}\n")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${test}-files" "${file_lines}")
    pn_add_test(
      EXECUTABLE
      NAME ${test}
      PREPEND_ENVIRONMENT ${test_env}
      COMMAND $<TARGET_FILE:${test}> "@${CMAKE_CURRENT_BINARY_DIR}/${test}-files")
  else ()
    pn_add_test(
      EXECUTABLE
      NAME ${test}
      COMMAND $<TARGET_FILE:${test}> -runs=1 ${CMAKE_CURRENT_SOURCE_DIR}/${test}>)
  endif ()
endmacro(pn_add_fuzz_test)

unset(fuzz_test_src)

# Fuzz tests at the User API level
pn_add_fuzz_test (fuzz-connection-driver fuzz-connection-driver.c)
target_link_libraries (fuzz-connection-driver ${FUZZING_QPID_PROTON_CORE_LIBRARY})
pn_add_fuzz_test (fuzz-message-decode fuzz-message-decode.c)
target_link_libraries (fuzz-message-decode ${FUZZING_QPID_PROTON_CORE_LIBRARY})

# pn_url_parse is not in proton core and is only used by messenger so compile specially
pn_add_fuzz_test (fuzz-url
  fuzz-url.c
  ${PN_C_SOURCE_DIR}/extra/url.c
  ${PN_C_SOURCE_DIR}/core/object/object.c
  ${PN_C_SOURCE_DIR}/core/object/string.c
  ${PN_C_SOURCE_DIR}/core/util.c
  ${PN_C_SOURCE_DIR}/core/memory.c)
target_compile_definitions(fuzz-url PRIVATE PROTON_DECLARE_STATIC)

# This regression test can take a very long time so don't run by default
if(HAS_PROACTOR AND FUZZ_LONG_TESTS)
  pn_add_fuzz_test (fuzz-proactor-receive fuzz-proactor-receive.c)
  target_link_libraries(fuzz-proactor-receive ${FUZZING_QPID_PROTON_CORE_LIBRARY} ${FUZZING_QPID_PROTON_PROACTOR_LIBRARY})
endif()

# Fuzz tests of internals

# pni_sniff_header is internal so it has to be compiled specially
pn_add_fuzz_test (fuzz-sniff-header fuzz-sniff-header.c ${PN_C_SOURCE_DIR}/core/autodetect.c)
