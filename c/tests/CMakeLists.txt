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

configure_file(test_config.h.in test_config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(test_env ${PN_ENV_SCRIPT} -- "PATH=$<TARGET_FILE_DIR:qpid-proton-core>")

# Add test without any qpid-proton lib: allows choice of
# qpid-proton-core or qpid-proton
macro (pn_add_c_test_nolib test)
  add_executable (${test} ${ARGN})
  target_link_libraries (${test} ${PLATFORM_LIBS})
  if (BUILD_WITH_CXX)
    set_source_files_properties (${ARGN} PROPERTIES LANGUAGE CXX)
  endif (BUILD_WITH_CXX)
  add_test(NAME ${test} COMMAND ${test_env} ${TEST_EXE_PREFIX_CMD} $<TARGET_FILE:${test}>)
endmacro (pn_add_c_test_nolib)

# Add test with qpid-proton-core linked
macro (pn_add_c_test test)
  pn_add_c_test_nolib (${test} ${ARGN})
  target_link_libraries (${test} qpid-proton-core ${PLATFORM_LIBS})
endmacro (pn_add_c_test)

pn_add_c_test (c-object-tests object.c)
pn_add_c_test (c-message-tests message.c)
pn_add_c_test (c-engine-tests engine.c)
pn_add_c_test (c-refcount-tests refcount.c)
pn_add_c_test (c-event-tests event.c)
pn_add_c_test (c-data-tests data.c)
pn_add_c_test (c-condition-tests condition.c)
pn_add_c_test (c-connection-driver-tests connection_driver.c)
pn_add_c_test (c-ssl-tests ssl.c)

pn_add_c_test_nolib (c-parse-url-tests parse-url.c)
target_link_libraries (c-parse-url-tests qpid-proton)

if(HAS_PROACTOR)
  pn_add_c_test (c-proactor-tests proactor.c)
  target_link_libraries (c-proactor-tests qpid-proton-proactor)

  # TODO Enable by default when races and xcode problems are cleared up
  option(THREADERCISER "Run the threaderciser concurrency tests" OFF)
  if (THREADERCISER)
    pn_add_c_test(c-threaderciser threaderciser.c)
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

  add_test(NAME c-fdlimit-tests COMMAND ${PN_ENV_SCRIPT} -- "PATH=${path}" "PYTHONPATH=${pypath}" ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/fdlimit.py)
endif(HAS_PROACTOR)

# fuzz tests: tests/fuzz
if (ENABLE_FUZZ_TESTING)
  add_subdirectory(fuzz)
endif (ENABLE_FUZZ_TESTING)
