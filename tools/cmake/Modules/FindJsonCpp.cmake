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

#.rst
# FindJsonCpp
#----------
#
# Find jsoncpp include directories and libraries.
#
# Sets the following variables:
#
#   JsonCpp_FOUND            - True if headers and requested libraries were found
#   JsonCpp_INCLUDE_DIRS     - JsonCpp include directories
#   JsonCpp_LIBRARIES        - Link these to use jsoncpp.
#
# This module reads hints about search locations from variables::
#   JSONCPP_ROOT             - Preferred installation prefix
#   JSONCPP_INCLUDEDIR       - Preferred include directory e.g. <prefix>/include
#   JSONCPP_LIBRARYDIR       - Preferred library directory e.g. <prefix>/lib

find_package (PkgConfig)
pkg_check_modules (PC_JsonCpp QUIET jsoncpp)

find_library(JsonCpp_LIBRARY NAMES jsoncpp libjsoncpp
  HINTS ${JSONCPP_LIBRARYDIR} ${JSONCPP_ROOT}/lib ${CMAKE_INSTALL_PREFIX}/lib
  PATHS ${PC_JsonCpp_LIBRARY_DIRS})

find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h json/value.h
  HINTS ${JSONCPP_INCLUDEDIR} ${JSONCPP_ROOT}/include ${CMAKE_INSTALL_PREFIX}/include
  PATHS /usr/include ${PC_JsonCpp_INCLUDE_DIRS})

set(JsonCpp_VERSION ${PC_JsonCpp_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JsonCpp
  REQUIRED_VARS JsonCpp_LIBRARY JsonCpp_INCLUDE_DIR
  VERSION_VAR JsonCpp_VERSION)

if (JsonCpp_FOUND)
  set(JsonCpp_INCLUDE_DIRS ${JsonCpp_INCLUDE_DIR})
  set(JsonCpp_LIBRARIES ${JsonCpp_LIBRARY})

  if (NOT TARGET JsonCpp::JsonCpp)
    add_library(JsonCpp::JsonCpp UNKNOWN IMPORTED)
    set_target_properties(JsonCpp::JsonCpp
      PROPERTIES
        IMPORTED_LOCATION "${JsonCpp_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${JsonCpp_INCLUDE_DIR}"
    )
  endif ()

endif ()

mark_as_advanced (JsonCpp_LIBRARY JsonCpp_INCLUDE_DIR)
