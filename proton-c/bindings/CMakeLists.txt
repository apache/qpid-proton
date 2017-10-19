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

# Add bindings that do not require swig here - the directory name must be the same as the binding name
# See below for swig bindings
set(BINDINGS javascript cpp go)

# Prerequisites for javascript.
#
# It uses a C/C++ to JavaScript cross-compiler called emscripten (https://github.com/kripken/emscripten). Emscripten takes C/C++
# and compiles it into a highly optimisable subset of JavaScript called asm.js (http://asmjs.org/) that can be
# aggressively optimised and run at near-native speed (usually between 1.5 to 10 times slower than native C/C++).
find_package(Emscripten)
if (EMSCRIPTEN_FOUND)
  set (DEFAULT_JAVASCRIPT ON)
endif (EMSCRIPTEN_FOUND)

if (CMAKE_CXX_COMPILER)
  set (DEFAULT_CPP ON)
endif()

# Prerequisites for Go
find_program(GO_EXE go)
mark_as_advanced(GO_EXE)
if (GO_EXE)
  if(WIN32)
    # Go on windows requires gcc tool chain
    set (DEFAULT_GO OFF)
  else()
    set (DEFAULT_GO ON)
  endif()
endif (GO_EXE)

if(SWIG_FOUND)
  # Add any new swig bindings here - the directory name must be the same as the binding name
  list(APPEND BINDINGS python ruby php perl)

  include(UseSWIG)

  # All swig modules should include ${PROTON_HEADERS} in SWIG_MODULE_<name>_EXTRA_DEPS
  file(GLOB PROTON_HEADERS "${CMAKE_SOURCE_DIR}/proton-c/include/proton/*.h")
  # All swig modules should include ${BINDING_DEPS} in swig_link_libraries
  set (BINDING_DEPS qpid-proton)

  # If swig version is 3.0 or greater then we can build some additional bindings
  if (${SWIG_VERSION} VERSION_GREATER 3.0 OR ${SWIG_VERSION} VERSION_EQUAL 3.0)
    list(APPEND SWIG_BINDINGS node)
  endif()

  # Add a block here to detect the prerequisites to build each language binding:
  #
  # If the prerequisites for the binding are present set a variable called
  # DEFAULT_{uppercase name of binding} to ON

  # Prerequisites for Python wrapper:
  find_package (PythonLibs ${PYTHON_VERSION_STRING} EXACT)
  if (PYTHONLIBS_FOUND)
    set (DEFAULT_PYTHON ON)
  endif (PYTHONLIBS_FOUND)

  # Prerequisites for Ruby:
  find_package(Ruby)
  if (RUBY_FOUND)
    set (DEFAULT_RUBY ON)
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
  include(ProtonFindPerl)
  if (PERLLIBS_FOUND)
    set (DEFAULT_PERL ON)
  endif (PERLLIBS_FOUND)

endif()


# To kick-start a build with just a few bindings enabled by default, e.g. ruby and go:
#
#     cmake -DBUILD_BINDINGS=ruby;go
#
# This is only used when CMakeCache.txt is first created, after that set the normal
# BUILD_XXX variables to enable/disable bindings.
#
if (NOT DEFINED BUILD_BINDINGS)
  set(BUILD_BINDINGS "${BINDINGS}")
endif()

foreach(BINDING ${BINDINGS})
  string(TOUPPER ${BINDING} UBINDING)
  list(FIND BUILD_BINDINGS ${BINDING} N)
  if(NOBUILD_${UBINDING} OR ( N EQUAL -1 ) ) # Over-ridden or not on the BUILD_BINDINGS list
    set(DEFAULT_${UBINDING} OFF)
  endif()
  option(BUILD_${UBINDING} "Build ${BINDING} language binding" ${DEFAULT_${UBINDING}})
  if (BUILD_${UBINDING})
    add_subdirectory(${BINDING})
  endif ()
endforeach(BINDING)

unset(BUILD_BINDINGS CACHE) # Remove from cache, only relevant when creating the initial cache.
