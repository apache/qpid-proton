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

include(${CMAKE_ROOT}/Modules/FindThreads.cmake)

if(Threads_FOUND AND NOT TARGET Threads::Threads)
  add_library(Threads::Threads UNKNOWN IMPORTED)

  if(THREADS_HAVE_PTHREAD_ARG)
    set_target_properties(Threads::Threads
      PROPERTIES
        INTERFACE_COMPILE_OPTIONS "-pthread"
    )
  endif()

  if(CMAKE_THREAD_LIBS_INIT)
    set_target_properties(Threads::Threads
      PROPERTIES
        IMPORTED_LOCATION "${CMAKE_THREAD_LIBS_INIT}"
    )
  endif()
endif()

