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

set (BINDING_DEPS qpid-proton qpid-proton-posix ${LINK_DEPS})

# Build wrapper for Python:
# @todo: conditionalize on whether python is available!
add_subdirectory(python)

# Build wrapper for PHP
# For now, assume PHP support if the 'php-config' tool is present.
# @todo: allow user to specify which php-config if multiple PHP sources installed!

FIND_PROGRAM(PHP_CONFIG_EXE php-config)
IF(PHP_CONFIG_EXE)
  add_subdirectory(php)
ENDIF(PHP_CONFIG_EXE)
