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

#.rst
# CheckPythonModule
# ----------------
#
# Provides a macro to check if a python module is available
#
# .. commmand:: CHECK_PYTHON_MODULE
#
#   ::
#
#     CHECK_PYTHON_MODULE(<module> <variable>)
#
#   Check if the given python ``<module>`` may be used by the detected
#   python interpreter and store the result in an internal cache entry
#   named ``<variable>``.
#
#   The ``Python_EXECUTABLE`` variable must be set before calling this
#   macro, usually by using find_package(Python).

macro (CHECK_PYTHON_MODULE MODULE VARIABLE)
  if (NOT ${VARIABLE} AND Python_EXECUTABLE)
    execute_process(
      COMMAND ${Python_EXECUTABLE} -c "import sys, pkgutil; sys.exit(0 if pkgutil.find_loader('${MODULE}') else 1)"
      RESULT_VARIABLE RESULT)
    if (RESULT EQUAL 0)
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for Python module ${MODULE} - found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have Python module ${MODULE}")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(STATUS "Looking for Python module ${MODULE} - not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have Python module ${MODULE}")
    endif()
  endif()
endmacro()
