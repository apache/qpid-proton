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

include_directories (${RUBY_INCLUDE_PATH})
swig_add_module(cproton-ruby ruby ruby.i)
swig_link_libraries(cproton-ruby ${BINDING_DEPS} ${RUBY_LIBRARY})
set_target_properties(cproton-ruby PROPERTIES PREFIX "")
add_custom_command(TARGET cproton-ruby
                   POST_BUILD
                   COMMAND cmake -E rename cproton-ruby.so cproton.so)

execute_process(COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "puts RbConfig::CONFIG['prefix']"
                OUTPUT_VARIABLE RUBY_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REPLACE "${RUBY_PREFIX}/" "" RUBY_PFX_ARCH_DIR ${RUBY_ARCH_DIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton.so
        RENAME cproton.so
        DESTINATION ${RUBY_PFX_ARCH_DIR}
        COMPONENT Ruby)
