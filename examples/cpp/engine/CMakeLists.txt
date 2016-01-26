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

find_package(ProtonCpp REQUIRED)

include_directories(${ProtonCpp_INCLUDE_DIRS})

foreach(example
    broker
    helloworld
    simple_recv
    simple_send
    direct_recv
    direct_send
    client
    server)
  add_executable(engine-${example} ${example}.cpp ${extra_source})
  target_link_libraries(engine-${example} ${ProtonCpp_LIBRARIES})
  set_source_files_properties(engine-${example}.cpp PROPERTIES COMPILE_FLAGS "${CXX_WARNING_FLAGS}")
  set_target_properties(engine-${example} PROPERTIES OUTPUT_NAME ${example})
endforeach()
