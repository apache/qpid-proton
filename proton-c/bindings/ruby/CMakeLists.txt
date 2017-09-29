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

set(src "${CMAKE_CURRENT_SOURCE_DIR}")
set(bin "${CMAKE_CURRENT_BINARY_DIR}")

## Build the swig library

list(APPEND SWIG_MODULE_cproton-ruby_EXTRA_DEPS
    ${CMAKE_SOURCE_DIR}/proton-c/include/proton/cproton.i
    ${PROTON_HEADERS}
)
include_directories (${RUBY_INCLUDE_PATH})
swig_add_library(cproton-ruby LANGUAGE ruby SOURCES ruby.i)
swig_link_libraries(cproton-ruby ${BINDING_DEPS} ${RUBY_LIBRARY})

# set a compiler macro to relay the Ruby version to the extension.
# Don't use the global CMAKE_C_FLAGS, -fvisibility=hidden causes an obscure
# failure with release builds.
string(REPLACE "." "" CFLAG_RUBY_VERSION "${RUBY_VERSION}")
string(SUBSTRING "${CFLAG_RUBY_VERSION}" 0 2 CFLAG_RUBY_VERSION)
set(CMAKE_C_FLAGS "-DRUBY${CFLAG_RUBY_VERSION}")

set_target_properties(cproton-ruby
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cproton"
    LINK_FLAGS "${CATCH_UNDEFINED}" )

##  Make a gem

file(GLOB_RECURSE RUBY_SRC RELATIVE . *.rb *.rdoc)

find_program(GEM_EXE gem DOC "Program to build and install ruby gem packages")
mark_as_advanced(GEM_EXE)
if (GEM_EXE)
  # Copy source and generated files to the build tree so we can build the gem in one place
  configure_file(${src}/qpid_proton.gemspec.in ${bin}/qpid_proton.gemspec)
  add_custom_command(
    OUTPUT ${bin}/qpid_proton-${PN_VERSION}.gem
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${src} ${bin}
    COMMAND ${CMAKE_COMMAND} -E copy ${bin}/rubyRUBY_wrap.c ${bin}/ext/cproton/cproton.c
    COMMAND ${GEM_EXE} build qpid_proton.gemspec
    DEPENDS ${RUBY_SRC} ${src}/LICENSE ${src}/TODO ${src}/ChangeLog ${bin}/rubyRUBY_wrap.c
    WORKING_DIRECTORY ${bin})

  add_custom_target(ruby-gem ALL DEPENDS ${bin}/qpid_proton-${PN_VERSION}.gem )
endif ()

## CMake-based install

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

install(TARGETS cproton-ruby DESTINATION ${RUBY_ARCHLIB_DIR} COMPONENT Ruby)
install(DIRECTORY lib DESTINATION ${RUBY_ARCHLIB_DIR} COMPONENT Ruby)

## Tests

set(bld "$<TARGET_FILE_DIR:qpid-proton>")

to_native_path("$ENV{RUBYLIB};${src}/lib;${src}/tests;${src}/spec;${bin};${bld}" RUBYLIB)
to_native_path("$ENV{PATH};${bin};${bld}" PATH)

execute_process(COMMAND ${RUBY_EXECUTABLE} -r minitest -e ""
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if (result EQUAL 0)  # Have minitest

  macro(add_ruby_test file)
    get_filename_component(name ${file} NAME_WE)
    string(REPLACE "_" "-" name "ruby-${name}")
    add_test(
      NAME ${name}
      COMMAND ${RUBY_EXECUTABLE} ${file} -v
      ${ARGN})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "PATH=${PATH};RUBYLIB=${RUBYLIB}")
  endmacro()
  add_ruby_test(example_test.rb WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples/ruby)

  file(GLOB TESTS tests/test_*.rb)
  file(GLOB SPECS spec/*_spec.rb)
  foreach(t ${TESTS} ${SPECS})
    add_ruby_test(${t})
  endforeach()
else()
  # No minitest
  message(WARNING "ruby tests will not run, minitest is not installed")
endif()

## Documentation

find_program(YARD_EXE "yard")
if (YARD_EXE)
  add_custom_command(
    OUTPUT ${bin}/doc
    WORKING_DIRECTORY ${src}
    COMMAND ${YARD_EXE} -q -o ${bin}/doc -b ${bin}/.yardoc --no-private -r README.rdoc
    DEPENDS ${RUBY_SRC}
    )
  add_custom_target(docs-ruby DEPENDS ${bin}/doc)
  add_dependencies (docs docs-ruby)
  install(DIRECTORY "${bin}/doc/"
    DESTINATION "${PROTON_SHARE}/docs/api-ruby"
    COMPONENT documentation
    OPTIONAL)
  set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES doc)
endif()
