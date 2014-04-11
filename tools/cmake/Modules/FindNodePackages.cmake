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

# FindNodePackages
# This module finds and installs (optionally) required node.js packages using npm
# * The ws package is the WebSocket library used by emscripten when the target is
#   node.js, it isn't needed for applications hosted on a browser where native 
#   WebSockets will be used.
#
# * The jsdoc package is a JavaScript API document generator analogous to Doxygen
#   or JavaDoc, it is used by the docs target in the JavaScript binding.

if (NOT NODE_PACKAGES_FOUND)
    # Install ws node.js WebSocket library https://github.com/einaros/ws
    message(STATUS "Installing node.js ws package")

    execute_process(
        COMMAND npm install ws
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE var
    )

    if (var)
        message(STATUS "Node.js ws package has been installed")
        set(NODE_WS_FOUND ON)
    endif (var)

    # Install jsdoc3 API documentation generator for JavaScript https://github.com/jsdoc3/jsdoc
    message(STATUS "Installing node.js jsdoc package")

    execute_process(
        COMMAND npm install jsdoc
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE var
    )

    if (var)
        message(STATUS "Node.js jsdoc package has been installed")
        set(NODE_JSDOC_FOUND ON)
        set(JSDOC_EXE ${PROJECT_SOURCE_DIR}/node_modules/.bin/jsdoc)
    endif (var)

    set(NODE_PACKAGES_FOUND ON)
endif (NOT NODE_PACKAGES_FOUND)

