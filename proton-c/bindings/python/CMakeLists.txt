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
set(CMAKE_SWIG_FLAGS "-threads")

include_directories (${PYTHON_INCLUDE_PATH})
if (BUILD_WITH_CXX)
   SET_SOURCE_FILES_PROPERTIES(python.i PROPERTIES CPLUSPLUS ON)
endif (BUILD_WITH_CXX)
swig_add_module(cproton python python.i)
swig_link_libraries(cproton ${BINDING_DEPS} ${PYTHON_LIBRARIES})
set_target_properties(_cproton
    PROPERTIES
    LINK_FLAGS "${CATCH_UNDEFINED}")

find_package(PythonInterp REQUIRED)

if (ASK_PYTHON)

  if (NOT PYTHON_SITEARCH_PACKAGES)
    execute_process(COMMAND ${PYTHON_EXECUTABLE}
      -c "from distutils.sysconfig import get_python_lib; print get_python_lib(True)"
      OUTPUT_VARIABLE PYTHON_ARCHLIB_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  else (NOT PYTHON_SITEARCH_PACKAGES)
    set (PYTHON_ARCHLIB_DIR "${PYTHON_SITEARCH_PACKAGES}")
  endif ()
else (ASK_PYTHON)
  set (PYTHON_ARCHLIB_DIR ${BINDINGS_DIR}/python/lib${LIB_SUFFIX} CACHE PATH "Python platform code")
endif (ASK_PYTHON)

install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -m py_compile cproton.py
                              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -O -m py_compile cproton.py
                              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -m py_compile proton.py
                              WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -O -m py_compile proton.py
                              WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")

find_program(EPYDOC_EXE epydoc)
if (EPYDOC_EXE)
   add_custom_target(docs-py COMMAND ${EPYDOC_EXE} -v --no-private --html
                     -o ${CMAKE_CURRENT_BINARY_DIR}/html
                     ${CMAKE_CURRENT_SOURCE_DIR}/proton.py)
   add_dependencies(docs docs-py)
   install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/"
           DESTINATION "${PROTON_SHARE}/docs/api-py"
           COMPONENT documentation
           ${OPTIONAL_ARG})
endif (EPYDOC_EXE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton.py
              ${CMAKE_CURRENT_BINARY_DIR}/cproton.pyc
              ${CMAKE_CURRENT_BINARY_DIR}/cproton.pyo
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.py
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.pyc
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.pyo
        DESTINATION ${PYTHON_ARCHLIB_DIR}
        COMPONENT Python)
install(TARGETS _cproton
        DESTINATION ${PYTHON_ARCHLIB_DIR}
        COMPONENT Python)
