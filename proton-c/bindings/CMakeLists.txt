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

set (BINDING_DEPS qpid-proton)

# Build wrapper for Python:
find_package (PythonLibs)
if (PYTHONLIBS_FOUND)
  add_subdirectory(python)
endif (PYTHONLIBS_FOUND)

# Build wrapper for Ruby:
find_package(Ruby)
if (RUBY_FOUND)
  add_subdirectory(ruby)
endif (RUBY_FOUND)

# Build wrapper for PHP
# For now, assume PHP support if the 'php-config' tool is present.
# @todo: allow user to specify which php-config if multiple PHP sources installed!
find_program(PHP_CONFIG_EXE php-config)
if (PHP_CONFIG_EXE)
  find_program(PHP_EXE php)
  if (PHP_EXE)
    add_subdirectory(php)
  endif (PHP_EXE)
endif (PHP_CONFIG_EXE)
