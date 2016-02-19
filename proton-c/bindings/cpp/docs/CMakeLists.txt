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

find_package(Doxygen)
if (DOXYGEN_FOUND)
  configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/user.doxygen.in
    ${CMAKE_CURRENT_BINARY_DIR}/user.doxygen)
  add_custom_target (docs-cpp
    COMMAND ${CMAKE_COMMAND} -E remove_directory html # get rid of old files
    COMMAND ${DOXYGEN_EXECUTABLE} user.doxygen)
  add_dependencies (docs docs-cpp)
  # HTML files are generated to ./html - put those in the install.
  install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/"
           DESTINATION "${PROTON_SHARE}/docs/api-cpp"
           COMPONENT documentation
           OPTIONAL)
endif (DOXYGEN_FOUND)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES html)
