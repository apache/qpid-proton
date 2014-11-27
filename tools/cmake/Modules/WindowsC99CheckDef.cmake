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

#
# Check qpid-proton.dll after linking for dangerous calls to
# Windows functions that suggest but deviate from C99 behavior:
#   _snprintf, vsnprintf, _vsnprintf
# See platform.h for safe wrapper calls.
#

set(obj_dir ${CMAKE_CURRENT_BINARY_DIR}/qpid-proton.dir/${CMAKE_CFG_INTDIR})

add_custom_command(TARGET qpid-proton PRE_LINK COMMAND ${PYTHON_EXECUTABLE}
        ${CMAKE_MODULE_PATH}WindowsC99SymbolCheck.py ${obj_dir}
        COMMENT "Checking for dangerous use of C99-violating functions")
