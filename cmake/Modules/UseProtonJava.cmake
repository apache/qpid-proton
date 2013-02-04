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

# Adds a custom command to rebuild the JAR to include resources and the
# directory entries that are missed by add_jar()

function (rebuild_jar upstream_target jar_name)
  add_custom_command(TARGET ${upstream_target} POST_BUILD
                     COMMAND ${Java_JAR_EXECUTABLE} cf ${jar_name}
                                -C ${CMAKE_CURRENT_SOURCE_DIR}/src/main/resources .
                                -C ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${upstream_target}.dir/ org
                     COMMENT "Rebuilding ${jar_name} to include missing resources")
endfunction ()

