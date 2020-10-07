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

# Note that this file is used *only* when building the examples within
# the proton source tree not when the examples are installed separately
# from it (for example in an OS distribution package).
#
# So if you find this file installed on your system something went wrong
# with the packaging and/or package installation.
#
# For a packaged installation the equivalent file is created by the source
# tree build and installed in the appropriate place for cmake on that system.

set (Proton_VERSION       ${PN_VERSION})

set (Proton_INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/c/include)
set (Proton_LIBRARIES     ${C_EXAMPLE_LINK_FLAGS} qpid-proton)
set (Proton_DEFINITIONS   ${C_EXAMPLE_FLAGS})
set (Proton_FOUND True)

set (Proton_Core_INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/c/include)
set (Proton_Core_LIBRARIES     ${C_EXAMPLE_LINK_FLAGS} qpid-proton-core)
set (Proton_Core_DEFINITIONS   ${C_EXAMPLE_FLAGS})

add_library(Proton::core ALIAS qpid-proton-core)

set (Proton_Core_FOUND True)

if (${HAS_PROACTOR})
  set (Proton_Proactor_INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/c/include)
  set (Proton_Proactor_LIBRARIES     ${C_EXAMPLE_LINK_FLAGS} qpid-proton-proactor)
  set (Proton_Proactor_DEFINITIONS   ${C_EXAMPLE_FLAGS})

  add_library(Proton::proactor ALIAS qpid-proton-proactor)

  set (Proton_Proactor_FOUND True)
endif()

# Check for all required components
foreach(comp ${Proton_FIND_COMPONENTS})
  if(NOT Proton_${comp}_FOUND)
    if(Proton_FIND_REQUIRED_${comp})
      set(Proton_FOUND FALSE)
      set(Proton_NOT_FOUND_MESSAGE "Didn't find required component ${comp}")
    endif()
  endif()
endforeach()
