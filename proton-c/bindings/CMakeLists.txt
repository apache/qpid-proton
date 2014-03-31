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

include(UseSWIG)

# Add any new bindings here - the directory name must be the same as the binding name
set (BINDINGS python ruby php perl)

set (BINDING_DEPS qpid-proton)

# Add a block here to detect the prerequisites to build each language binding:
#
# If the prerequisites for the binding are present set a variable called
# DEFAULT_{uppercase name of binding} to ON

# Prerequisites for Python wrapper:
find_package (PythonLibs)
if (PYTHONLIBS_FOUND)
  set (DEFAULT_PYTHON ON)
endif (PYTHONLIBS_FOUND)

# Prerequisites for Ruby:
find_program(GEM_EXE "gem")
macro(CheckRubyGem varname gemname)
  execute_process(COMMAND ${GEM_EXE} list --local ${gemname}
    OUTPUT_VARIABLE CHECK_OUTPUT)

  set (${varname} OFF)

  if (CHECK_OUTPUT MATCHES "${gemname}[ ]+\(.*\)")
    message(STATUS "Found Ruby gem: ${gemname}")
    set (${varname} ON)
  else()
    message(STATUS "Missing Ruby gem dependency: ${gemname}")
    set (${varname} OFF)
  endif()
endmacro()

find_package(Ruby)
if (RUBY_FOUND)
  set (DEFAULT_RUBY ON)

  CheckRubyGem("HAS_RUBY_GEM_RSPEC"     "rspec")
  CheckRubyGem("HAS_RUBY_GEM_SIMPLECOV" "simplecov")

  if (HAS_RUBY_GEM_RSPEC AND HAS_RUBY_GEM_SIMPLECOV)
    set (DEFAULT_RUBY_TESTING ON CACHE INTERNAL "")
  else()
    set (DEFAULT_RUBY_TESTING OFF CACHE INTERNAL "")
  endif (HAS_RUBY_GEM_RSPEC AND HAS_RUBY_GEM_SIMPLECOV)
endif (RUBY_FOUND)

# Prerequites for PHP:
# For now, assume PHP support if the 'php-config' tool is present.
# @todo: allow user to specify which php-config if multiple PHP sources installed!
find_program(PHP_CONFIG_EXE php-config)
if (PHP_CONFIG_EXE)
  find_program(PHP_EXE php)
  mark_as_advanced (PHP_EXE)
  if (PHP_EXE)
    set (DEFAULT_PHP ON)
  endif (PHP_EXE)
endif (PHP_CONFIG_EXE)
mark_as_advanced (PHP_CONFIG_EXE)

# Prerequisites for Perl:
include(FindPerl)
include(FindPerlLibs)
if (PERLLIBS_FOUND)
  set (DEFAULT_PERL ON)
endif (PERLLIBS_FOUND)

# Shouldn't need to modify below here when adding new language binding
foreach(BINDING ${BINDINGS})
  string(TOUPPER ${BINDING} UBINDING)
  # Check whether default was overridden
  if ("NOBUILD_${UBINDING}")
    set ("DEFAULT_${UBINDING}" OFF)
  endif ()
  option("BUILD_${UBINDING}" "Build ${BINDING} language binding" ${DEFAULT_${UBINDING}})
  if ("BUILD_${UBINDING}")
    add_subdirectory(${BINDING})
  endif ()
endforeach(BINDING)
