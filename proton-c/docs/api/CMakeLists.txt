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
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/user.doxygen.in
                  ${CMAKE_CURRENT_BINARY_DIR}/user.doxygen)
  add_custom_target (docs COMMAND ${DOXYGEN_EXECUTABLE} user.doxygen)

  # HTML files are generated to ./html - put those in the install.
  install (DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
           DESTINATION ${PROTON_SHARE}/docs/api
           COMPONENT documentation
           OPTIONAL)
  # if (CPACK_GENERATOR STREQUAL "NSIS")
  #     set (CPACK_NSIS_MENU_LINKS
  #          "${QPID_INSTALL_HTMLDIR}/index.html" "Qpid C++ API Documentation"
  #          "https://issues.apache.org/jira/browse/QPID" "Report Qpid Problem")
  # endif (CPACK_GENERATOR STREQUAL "NSIS")
endif (DOXYGEN_FOUND)
