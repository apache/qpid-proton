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

if (NOT DEFAULT_RUBY_TESTING)
  message(FATAL_ERROR "Ruby bindings cannot be tested while missing dependencies")
endif (NOT DEFAULT_RUBY_TESTING)

include_directories (${RUBY_INCLUDE_PATH})
swig_add_module(cproton-ruby ruby ruby.i)
swig_link_libraries(cproton-ruby ${BINDING_DEPS} ${RUBY_LIBRARY})
set_target_properties(cproton-ruby
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cproton"
    LINK_FLAGS "${CATCH_UNDEFINED}" )

if (CHECK_SYSINSTALL_RUBY)
  execute_process(COMMAND ${RUBY_EXECUTABLE}
    -r rbconfig -e "print RbConfig::CONFIG['vendorarchdir'] || ''"
    RESULT_VARIABLE RESULT_RUBY_ARCHLIB_DIR
    OUTPUT_VARIABLE OUTPUT_RUBY_ARCHLIB_DIR)

  if(OUTPUT_RUBY_ARCHLIB_DIR STREQUAL "")
    execute_process(COMMAND ${RUBY_EXECUTABLE}
      -r rbconfig -e "print RbConfig::CONFIG['archdir'] || ''"
      RESULT_VARIABLE RESULT_RUBY_ARCHLIB_DIR
      OUTPUT_VARIABLE OUTPUT_RUBY_ARCHLIB_DIR)
  endif()

  set(RUBY_ARCHLIB_DIR_DEFAULT "${OUTPUT_RUBY_ARCHLIB_DIR}")
else (CHECK_SYSINSTALL_RUBY)
  set (RUBY_ARCHLIB_DIR_DEFAULT ${BINDINGS_DIR}/ruby)
endif (CHECK_SYSINSTALL_RUBY)

if (NOT RUBY_ARCHLIB_DIR)
  set (RUBY_ARCHLIB_DIR ${RUBY_ARCHLIB_DIR_DEFAULT})
endif()

install(TARGETS cproton-ruby
        DESTINATION ${RUBY_ARCHLIB_DIR}
        COMPONENT Ruby)
install(FILES lib/qpid_proton.rb
        DESTINATION ${RUBY_ARCHLIB_DIR}
        COMPONENT Ruby)
install(DIRECTORY lib/qpid_proton
        DESTINATION ${RUBY_ARCHLIB_DIR}
        COMPONENT Ruby)
