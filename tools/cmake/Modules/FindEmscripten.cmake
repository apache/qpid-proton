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

# FindEmscripten
# This module checks if Emscripten and its prerequisites are installed and if so
# sets EMSCRIPTEN_FOUND Emscripten (https://github.com/kripken/emscripten) is a
# C/C++ to JavaScript cross-compiler used to generate the JavaScript bindings.

if (NOT EMSCRIPTEN_FOUND)
    # First check that Node.js is installed as that is needed by Emscripten.
    find_program(NODE node)
    if (NOT NODE)
        message(STATUS "Node.js (http://nodejs.org) is not installed: can't build JavaScript binding")
    else (NOT NODE)
        # Check that the Emscripten C/C++ to JavaScript cross-compiler is installed.
        find_program(EMCC emcc)
        if (NOT EMCC)
            message(STATUS "Emscripten (https://github.com/kripken/emscripten) is not installed: can't build JavaScript binding")
        else (NOT EMCC)
            set(EMSCRIPTEN_FOUND ON)
        endif (NOT EMCC)
    endif (NOT NODE)
endif (NOT EMSCRIPTEN_FOUND)

