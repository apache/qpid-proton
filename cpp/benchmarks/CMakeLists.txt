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

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(benchmark REQUIRED)

# incompatibility in older libbenchmark, https://github.com/google/benchmark/issues/494#issuecomment-502444478
get_source_file_property(COMPILE_FLAGS benchmarks_main.cpp current_compile_flags)
set_source_files_properties(benchmarks_main.cpp PROPERTIES COMPILE_FLAGS "${current_compile_flags} -Wno-pedantic")

add_executable(cpp-benchmarks benchmarks_main.cpp container.cpp)
target_link_libraries(cpp-benchmarks benchmark pthread qpid-proton-cpp)

add_test(NAME cpp-benchmarks COMMAND cpp-benchmarks)
