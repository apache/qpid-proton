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

if (ENABLE_VALGRIND AND VALGRIND_EXE)
  set(memcheck-cmd ${VALGRIND_EXE} --error-exitcode=1 --quiet
                   --leak-check=full --trace-children=yes)
endif ()

macro (pn_add_c_test test file)
  add_executable (${test} ${file})
  target_link_libraries (${test} qpid-proton)
  if (BUILD_WITH_CXX)
    set_source_files_properties (${file} PROPERTIES LANGUAGE CXX)
  endif (BUILD_WITH_CXX)
  if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_test (NAME ${test}
              COMMAND ${env_py}
                "PATH=$<TARGET_FILE_DIR:qpid-proton>"
                $<TARGET_FILE:${test}>)
  else ()
    add_test (${test} ${memcheck-cmd} ${CMAKE_CURRENT_BINARY_DIR}/${test})
  endif ()
endmacro(pn_add_c_test)

pn_add_c_test (c-object-tests object.c)
pn_add_c_test (c-message-tests message.c)
pn_add_c_test (c-engine-tests engine.c)
pn_add_c_test (c-parse-url-tests parse-url.c)
pn_add_c_test (c-refcount-tests refcount.c)
pn_add_c_test (c-reactor-tests reactor.c)
pn_add_c_test (c-event-tests event.c)
pn_add_c_test (c-data-tests data.c)
