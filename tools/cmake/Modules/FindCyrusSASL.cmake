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
# FindCyrusSASL
#--------------
#
# Find Cyrus SASL include dirs and libraries.
#
# Sets the following variables:
#
#   CyrusSASL_FOUND            - True if headers and requested libraries were found
#   CyrusSASL_INCLUDE_DIRS     - Cyrus SASL include directories
#   CyrusSASL_LIBRARIES        - Link these to use Cyrus SASL library.
#

find_package (PkgConfig)
pkg_check_modules (PC_CyrusSASL QUIET libsasl2)

find_library (CyrusSASL_LIBRARY
  NAMES sasl2
  PATHS ${PC_CyrusSASL_LIBRARY_DIRS}
)
find_path (CyrusSASL_INCLUDE_DIR
  NAMES sasl/sasl.h
  PATHS ${PC_CyrusSASL_INCLUDE_DIRS}
  PATH_SUFFIXES include
)

set (CyrusSASL_VERSION ${PC_CyrusSASL_VERSION})

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (CyrusSASL
  FOUND_VAR CyrusSASL_FOUND
  REQUIRED_VARS CyrusSASL_LIBRARY CyrusSASL_INCLUDE_DIR
  VERSION_VAR CyrusSASL_VERSION
)

if (CyrusSASL_FOUND)
  set (CyrusSASL_LIBRARIES ${CyrusSASL_LIBRARY})
  set (CyrusSASL_INCLUDE_DIRS ${CyrusSASL_INCLUDE_DIR})

  if (NOT TARGET CyrusSASL::CyrusSASL)
    add_library(CyrusSASL::CyrusSASL UNKNOWN IMPORTED)
    set_target_properties(CyrusSASL::CyrusSASL
      PROPERTIES
        IMPORTED_LOCATION "${CyrusSASL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CyrusSASL_INCLUDE_DIR}"
    )
  endif ()

  # Find saslpasswd2 executable to generate test config
  find_program (CyrusSASL_Saslpasswd_EXECUTABLE
    NAMES saslpasswd2
    DOC "Program used to make the Cyrus SASL user database")

  mark_as_advanced (CyrusSASL_Saslpasswd_EXECUTABLE)

endif ()

mark_as_advanced (CyrusSASL_LIBRARY CyrusSASL_INCLUDE_DIR)

