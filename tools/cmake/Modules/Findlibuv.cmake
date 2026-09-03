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
# FindLibuv
#----------
#
# Find libuv include dirs and libraries.
#
# Sets the following variables:
#
#   libuv_FOUND            - True if headers and requested libraries were found
#   libuv_INCLUDE_DIRS     - Libuv include directories
#   libuv_LIBRARIES        - Link these to use libuv.
#
# This module reads hints about search locations from variables::
#   LIBUV_ROOT             - Preferred installation prefix
#   LIBUV_INCLUDEDIR       - Preferred include directory e.g. <prefix>/include
#   LIBUV_LIBRARYDIR       - Preferred library directory e.g. <prefix>/lib

find_package (PkgConfig)
pkg_check_modules (PC_Libuv QUIET libuv)

find_library(libuv_LIBRARY NAMES uv libuv
  HINTS ${LIBUV_LIBRARYDIR} ${LIBUV_ROOT}/lib ${CMAKE_INSTALL_PREFIX}/lib
  PATHS ${PC_Libuv_LIBRARY_DIRS})

find_path(libuv_INCLUDE_DIR NAMES uv.h
  HINTS ${LIBUV_INCLUDEDIR} ${LIBUV_ROOT}/include ${CMAKE_INSTALL_PREFIX}/include
  PATHS /usr/include ${PC_Libuv_INCLUDE_DIRS})

set(libuv_VERSION ${PC_Libuv_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libuv
  FOUND_VAR libuv_FOUND
  REQUIRED_VARS libuv_LIBRARY libuv_INCLUDE_DIR
  VERSION_VAR libuv_VERSION)

if (libuv_FOUND)
  set(libuv_INCLUDE_DIRS ${libuv_INCLUDE_DIR})
  set(libuv_LIBRARIES ${libuv_LIBRARY})

  if (NOT TARGET libuv::uv)
    add_library(libuv::uv UNKNOWN IMPORTED)
    set_target_properties(libuv::uv
      PROPERTIES
        IMPORTED_LOCATION "${libuv_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${libuv_INCLUDE_DIR}"
    )
  endif ()

endif ()

mark_as_advanced (libuv_LIBRARY libuv_INCLUDE_DIR)
