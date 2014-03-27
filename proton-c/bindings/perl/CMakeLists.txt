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

if (CHECK_SYSINSTALL_PERL)
  execute_process(COMMAND perl -V:installvendorarch
    OUTPUT_VARIABLE PERL_VENDORARCH_OUTPUT_VARIABLE
    RESULT_VARIABLE PERL_VENDORARCH_RESULT_VARIABLE)

  if (NOT PERL_VENDORARCH_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_VENDORARCH ${PERL_VENDORARCH_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_VENDORARCH}" PERL_VENDORARCH_DIR_DEFAULT)
  else ()
    set (PERL_VENDORARCH_DIR_DEFAULT ${PERL_VENDORARCH_RESULT_VARIABLE})
  endif ()
else (CHECK_SYSINSTALL_PERL)
  set (PERL_VENDORARCH_DIR_DEFAULT ${BINDINGS_DIR}/perl)
endif (CHECK_SYSINSTALL_PERL)

if (NOT PERL_VENDORARCH_DIR)
  set (PERL_VENDORARCH_DIR ${PERL_VENDORARCH_DIR_DEFAULT})
endif()

set (CMAKE_C_FLAGS ${PERLCFLAGS})

swig_add_module(cproton_perl perl perl.i)
swig_link_libraries(cproton_perl ${BINDING_DEPS} ${PERL_LIBRARY})

if ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.so
        DESTINATION ${PERL_VENDORARCH_DIR}
        COMPONENT Perl
        )
else()
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproton_perl.so
        RENAME cproton_perl.so
        DESTINATION ${PERL_VENDORARCH_DIR}
        COMPONENT Perl
        )
endif ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.pm
        DESTINATION ${PERL_VENDORARCH_DIR}
        COMPONENT Perl)

install(FILES lib/qpid_proton.pm
        DESTINATION ${PERL_VENDORARCH_DIR}
        COMPONENT Perl)

INSTALL(DIRECTORY lib/qpid
        DESTINATION ${PERL_VENDORARCH_DIR}
        COMPONENT Perl)
