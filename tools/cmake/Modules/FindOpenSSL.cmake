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

# This is a wrapper hack purely so that we can use imported targets
# with cmake 2.8.12 and its supplied older modules

include(${CMAKE_ROOT}/Modules/FindOpenSSL.cmake)

if (OPENSSL_FOUND AND NOT TARGET OpenSSL::SSL)
  add_library(OpenSSL::SSL UNKNOWN IMPORTED)
  set_target_properties(OpenSSL::SSL
    PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
      INTERFACE_LINK_LIBRARIES "${OPENSSL_CRYPTO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
  )
endif ()


