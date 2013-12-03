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
set_target_properties(cproton-ruby
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cproton"
    LINK_FLAGS "${CATCH_UNDEFINED}" )

# Install the Ruby libraries
set(RUBY_LIBRARIES
    lib/qpid_proton/array.rb
    lib/qpid_proton/data.rb
    lib/qpid_proton/described.rb
    lib/qpid_proton/exception_handling.rb
    lib/qpid_proton/exceptions.rb
    lib/qpid_proton/hash.rb
    lib/qpid_proton/mapping.rb
    lib/qpid_proton/message_format.rb
    lib/qpid_proton/message.rb
    lib/qpid_proton/messenger.rb
    lib/qpid_proton/subscription.rb
    lib/qpid_proton/tracker.rb
    lib/qpid_proton/tracker_status.rb
)

if (NOT RUBY_INSTALL_PREFIX)
  set(RUBY_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif (NOT RUBY_INSTALL_PREFIX)

# if the CMAKE_INSTALL_PREFIX isn't defined, then set it
# using the prefix from Ruby
execute_process(COMMAND ${RUBY_EXECUTABLE}
  -r rbconfig -e "print RbConfig::CONFIG['prefix']"
  OUTPUT_VARIABLE RUBY_PREFIX)

set(RUBY_ARCHLIB_DIR "${RUBY_VENDORLIB_DIR}")

# Get the Ruby arch directory for where platform-specific code
execute_process(COMMAND ${RUBY_EXECUTABLE}
  -r rbconfig -e "install_prefix = '${RUBY_INSTALL_PREFIX}';
                                  path = RbConfig::CONFIG['vendorarchdir'];
                                  unless path.nil?
                                    path.gsub!('${RUBY_PREFIX}',
                                               install_prefix);
                                  else
                                    path = '';
                                  end;
                                  print path;"
  RESULT_VARIABLE RESULT_RUBY_ARCHLIB_DIR
  OUTPUT_VARIABLE OUTPUT_RUBY_ARCHLIB_DIR)

# fallback for older versions of Ruby
if(OUTPUT_RUBY_ARCHLIB_DIR STREQUAL "")
  execute_process(COMMAND ${RUBY_EXECUTABLE}
    -r rbconfig -e "install_prefix = '${RUBY_INSTALL_PREFIX}';
                                    path = RbConfig::CONFIG['archdir'];
                                    path.gsub!('${RUBY_PREFIX}',
                                               install_prefix);
                                    print path;"
    RESULT_VARIABLE RESULT_RUBY_ARCHLIB_DIR
    OUTPUT_VARIABLE OUTPUT_RUBY_ARCHLIB_DIR)
endif (OUTPUT_RUBY_ARCHLIB_DIR STREQUAL "")


set(RUBY_ARCHLIB_DIR "${OUTPUT_RUBY_ARCHLIB_DIR}")

MESSAGE(STATUS "RUBY_ARCHLIB_DIR=${RUBY_ARCHLIB_DIR}")

# Get the Ruby library directory for platform independent code
execute_process(COMMAND ${RUBY_EXECUTABLE}
  -r rbconfig -e "install_prefix = '${RUBY_INSTALL_PREFIX}';
                                  path = RbConfig::CONFIG['vendorlibdir'];
                                  unless path.nil?
                                    path.gsub!('${RUBY_PREFIX}',
                                               install_prefix);
                                  else
                                    path = '';
                                  end;
                                  print path;"
  RESULT_VARIABLE RESULT_RUBY_SITELIB_DIR
  OUTPUT_VARIABLE OUTPUT_RUBY_SITELIB_DIR)

# fallback for older versions of Ruby
if(OUTPUT_RUBY_SITELIB_DIR STREQUAL "")
  execute_process(COMMAND ${RUBY_EXECUTABLE}
    -r rbconfig -e "install_prefix = '${RUBY_INSTALL_PREFIX}';
                                    path = RbConfig::CONFIG['libdir'];
                                    path.gsub!('${RUBY_PREFIX}',
                                               install_prefix);
                                    print path;"
    RESULT_VARIABLE RESULT_RUBY_SITELIB_DIR
    OUTPUT_VARIABLE OUTPUT_RUBY_SITELIB_DIR)

endif(OUTPUT_RUBY_SITELIB_DIR STREQUAL "")

set(RUBY_SITELIB_DIR "${OUTPUT_RUBY_SITELIB_DIR}")

MESSAGE(STATUS "RUBY_SITELIB_DIR=${RUBY_SITELIB_DIR}")

install(FILES lib/qpid_proton.rb
        DESTINATION ${RUBY_SITELIB_DIR}
        COMPONENT Ruby)

install(FILES ${RUBY_LIBRARIES}
        DESTINATION ${RUBY_SITELIB_DIR}/qpid_proton
        COMPONENT Ruby)

install(TARGETS cproton-ruby
        DESTINATION ${RUBY_ARCHLIB_DIR}
        COMPONENT Ruby)

