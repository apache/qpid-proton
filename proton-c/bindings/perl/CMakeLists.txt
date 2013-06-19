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

include_directories("${PERL_INCLUDE_PATH}")

# Need to pass in the same compiler flags used to build Perl itself
execute_process(COMMAND perl -MConfig -e "print \$Config{ccflags}"
                OUTPUT_VARIABLE PERLCFLAGS)
set (CMAKE_C_FLAGS ${PERLCFLAGS})

swig_add_module(cproton_perl perl perl.i)
swig_link_libraries(cproton_perl ${BINDING_DEPS} ${PERL_LIBRARY})

if ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.so
        DESTINATION ${PERL_ARCHLIB}
        COMPONENT ${QPID_COMPONENT_CLIENT}
        )
else()
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproton_perl.so
        RENAME cproton_perl.so
        DESTINATION ${PERL_ARCHLIB}
        COMPONENT ${QPID_COMPONENT_CLIENT}
        )
endif ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))


install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.pm
        DESTINATION ${PERL_ARCHLIB}
        COMPONENT Perl)

