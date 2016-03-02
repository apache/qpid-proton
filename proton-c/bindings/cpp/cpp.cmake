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

# Check C++ capabilities.

include(CheckCXXSourceCompiles)

if (CMAKE_CXX_COMPILER)
  set(CMAKE_REQUIRED_FLAGS "${CXX_WARNING_FLAGS}")
  check_cxx_source_compiles("long long ll; int main(int, char**) { return 0; }" HAS_LONG_LONG)
  if (HAS_LONG_LONG)
    add_definitions(-DPN_CPP_HAS_LONG_LONG=1)
  endif()
  check_cxx_source_compiles("#include <memory>\nstd::shared_ptr<int> i; std::unique_ptr<int> u; int main(int, char**) { return 0; }" HAS_STD_PTR)
  if (HAS_STD_PTR)
    add_definitions(-DPN_CPP_HAS_STD_PTR=1)
  endif()
endif()
