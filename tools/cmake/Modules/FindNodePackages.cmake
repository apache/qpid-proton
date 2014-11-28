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
# This module finds and installs (using npm) node.js packages that are used by
# the JavaScript binding. The binding should still compile if these packages
# cannot be installed but certain features might not work as described below.
#
# * The ws package is the WebSocket library used by emscripten when the target is
#   node.js, it isn't needed for applications hosted on a browser (where native 
#   WebSockets will be used), but without it it won't work with node.js.
#
# * The jsdoc package is a JavaScript API document generator analogous to Doxygen
#   or JavaDoc, it is used by the docs target in the JavaScript binding.

if (NOT NODE_PACKAGES_FOUND)
    # Check if the specified node.js package is already installed, if not install it.
    macro(InstallPackage varname name)
        execute_process(
            COMMAND npm list --local ${name}
            OUTPUT_VARIABLE check_package
        )

        set(${varname} OFF)

        if (check_package MATCHES "${name}@.")
            message(STATUS "Found node.js package: ${name}")
            set(${varname} ON)
        else()
            message(STATUS "Installing node.js package: ${name}")

            execute_process(
                COMMAND npm install ${name}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE var
            )

            if (var)
                message(STATUS "Installed node.js package: ${name}")
                set(${varname} ON)
            endif (var)
        endif()
    endmacro()

    # Check if ws WebSocket library https://github.com/einaros/ws is installed
    InstallPackage("NODE_WS_FOUND" "ws")

    # Check if jsdoc3 API documentation generator https://github.com/jsdoc3/jsdoc is installed
    InstallPackage("NODE_JSDOC_FOUND" "jsdoc")

    set(NODE_PACKAGES_FOUND ON)
endif (NOT NODE_PACKAGES_FOUND)

