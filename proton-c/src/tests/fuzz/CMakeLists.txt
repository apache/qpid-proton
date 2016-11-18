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

add_library (StandaloneFuzzTargetMain StandaloneFuzzTargetMain.c)
if (BUILD_WITH_CXX)
  # ignore the flag for this file, it complicates things for no gain
endif (BUILD_WITH_CXX)

macro (pn_add_fuzz_test test)
  add_executable (${test} ${ARGN})
  if (FUZZING_ENGINE)
    target_link_libraries (${test} qpid-proton FuzzingEngine)
  else (FUZZING_ENGINE)
    target_link_libraries (${test} qpid-proton StandaloneFuzzTargetMain)
  endif (FUZZING_ENGINE)
  if (BUILD_WITH_CXX)
    set_source_files_properties (${ARGN} PROPERTIES LANGUAGE CXX)
  else (BUILD_WITH_CXX)
    set_target_properties(${test} PROPERTIES LINKER_LANGUAGE CXX)
  endif (BUILD_WITH_CXX)
  
  if (FUZZING_ENGINE)
    add_test (${test} ${CMAKE_CURRENT_BINARY_DIR}/${test} -runs=1 ${CMAKE_CURRENT_SOURCE_DIR}/${test})
  else (FUZZING_ENGINE)
    # StandaloneFuzzTargetMain cannot walk directory trees
    file(GLOB_RECURSE files ${CMAKE_CURRENT_SOURCE_DIR}/${test}/*)
    add_test (${test} ${CMAKE_CURRENT_BINARY_DIR}/${test} ${files})
  endif (FUZZING_ENGINE)
endmacro(pn_add_fuzz_test)

pn_add_fuzz_test (fuzz-connection-driver fuzz-connection-driver.c)
pn_add_fuzz_test (fuzz-message-decode fuzz-message-decode.c)
# pni_sniff_header is internal, it has to be compiled in anew
pn_add_fuzz_test (fuzz-sniff-header fuzz-sniff-header.c ${PROJECT_SOURCE_DIR}/proton-c/src/core/autodetect.c)
pn_add_fuzz_test (fuzz-url fuzz-url.c)
if(HAS_PROACTOR)
  pn_add_fuzz_test (fuzz-proactor-receive fuzz-proactor-receive.c)
endif(HAS_PROACTOR)
