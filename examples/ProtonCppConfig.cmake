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

set (ProtonCpp_VERSION       ${PN_VERSION})
set (ProtonCpp_INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/proton-c/include ${CMAKE_SOURCE_DIR}/proton-c/bindings/cpp/include)
set (ProtonCpp_LIBRARIES     qpid-proton-cpp)
set (ProtonCpp_FOUND True)
