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

cmake_minimum_required(VERSION 2.8.12)

project(ProtonExamples)
include(CTest)
enable_testing()
include("tests/RuntimeCheck.cmake")

# ind example sub-directories that contain "CMakeLists.txt" or "testme"
set(ex_dir "${CMAKE_SOURCE_DIR}/examples")
file(GLOB subs ${ex_dir}/*)
foreach(dir ${subs})
  get_filename_component(ex "${dir}" NAME)
  if(EXISTS ${dir}/CMakeLists.txt)
    # Has CMakeLists.txt to define example tests.
    add_subdirectory(${dir})
  elseif(EXISTS ${dir}/testme)
    # Has a "testme" script to run example tests.
    add_test(
      NAME ${ex}-example-tests
      COMMAND ${dir}/testme
      WORKING_DIRECTORY ${dir})
  endif()
 endforeach()
