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

if (ASK_PERL)

  execute_process(COMMAND perl -V:installvendorlib
    OUTPUT_VARIABLE PERL_ARCHLIB_OUTPUT_VARIABLE
    RESULT_VARIABLE PERL_ARCHLIB_RESULT_VARIABLE)
  if (NOT PERL_ARCHLIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_ARCHLIB ${PERL_ARCHLIB_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_ARCHLIB}" PERL_ARCHLIB_DIR)
  endif (NOT PERL_ARCHLIB_RESULT_VARIABLE)

  execute_process(COMMAND perl -V:installsitelib
    OUTPUT_VARIABLE PERL_SITELIB_OUTPUT_VARIABLE
    RESULT_VARIABLE PERL_SITELIB_RESULT_VARIABLE)
  if (NOT PERL_SITELIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_SITELIB ${PERL_SITELIB_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_SITELIB}" PERL_SITELIB_DIR)
  endif (NOT PERL_SITELIB_RESULT_VARIABLE)

else (ASK_PERL)
  set (PERL_ARCHLIB_DIR ${BINDINGS_DIR}/perl/lib${LIB_SUFFIX} CACHE PATH "Perl platform code")
  set (PERL_SITELIB_DIR ${BINDINGS_DIR}/perl/ CACHE PATH "Perl code")
endif (ASK_PERL)

set (CMAKE_C_FLAGS ${PERLCFLAGS})

swig_add_module(cproton_perl perl perl.i)
swig_link_libraries(cproton_perl ${BINDING_DEPS} ${PERL_LIBRARY})

if ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.so
        DESTINATION ${PERL_ARCHLIB_DIR}
        COMPONENT ${QPID_COMPONENT_CLIENT}
        )
else()
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproton_perl.so
        RENAME cproton_perl.so
        DESTINATION ${PERL_ARCHLIB_DIR}
        COMPONENT ${QPID_COMPONENT_CLIENT}
        )
endif ((${CMAKE_MAJOR_VERSION} EQUAL 2) AND (${CMAKE_MINOR_VERSION} LESS 8))

# get the perl vendor library if it's not already defined
#if (NOT PERL_ARCHLIB)
#  execute_process(COMMAND ${PERL_EXECUTABLE} "-V:installvendorlib"
#                  OUTPUT_VARIABLE PERL_ARCHLIB_OUTPUT_VARIABLE
#                  RESULT_VARIABLE PERL_ARCHLIB_RESULT_VARIABLE)
#endif (!DEFINED PERL_ARCHLIB)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton_perl.pm
        DESTINATION ${PERL_ARCHLIB_DIR}
        COMPONENT Perl)

install(FILES lib/qpid_proton.pm
        DESTINATION ${PERL_SITELIB_DIR}
        COMPONENT Perl)

INSTALL(DIRECTORY lib/qpid
        DESTINATION ${PERL_SITELIB_DIR}
        COMPONENT Perl)
