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

# NB For python the SWIG module name must have the same name as the input .i file for CMake to generate the
# correct dependencies

set(CMAKE_SWIG_FLAGS "-threads")

include_directories (${PYTHON_INCLUDE_PATH})

set_source_files_properties(cproton.i PROPERTIES CPLUSPLUS NO)

list(APPEND SWIG_MODULE_cproton_EXTRA_DEPS
    ${CMAKE_SOURCE_DIR}/proton-c/include/proton/cproton.i
    ${PROTON_HEADERS}
)

swig_add_module(cproton python cproton.i)
swig_link_libraries(cproton ${BINDING_DEPS} ${PYTHON_LIBRARIES})
set_target_properties(${SWIG_MODULE_cproton_REAL_NAME}
    PROPERTIES
    LINK_FLAGS "${CATCH_UNDEFINED}")

find_package(PythonInterp REQUIRED)

if (CHECK_SYSINSTALL_PYTHON)
  execute_process(COMMAND ${PYTHON_EXECUTABLE}
    -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(True))"
    OUTPUT_VARIABLE PYTHON_SITEARCH_PACKAGES_DEFAULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
else ()
  set (PYTHON_SITEARCH_PACKAGES_DEFAULT ${BINDINGS_DIR}/python)
endif ()

if (NOT PYTHON_SITEARCH_PACKAGES)
  set (PYTHON_SITEARCH_PACKAGES ${PYTHON_SITEARCH_PACKAGES_DEFAULT})
endif()

set (pysrc-generated cproton.py)
set (pysrc
    proton/__init__.py
    proton/handlers.py
    proton/reactor.py
    proton/utils.py
    proton/wrapper.py
    proton/_compat.py
    )

macro (py_compile directory files artifacts)
  foreach (src_file ${files})
    install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}c')\"
                                  WORKING_DIRECTORY ${directory})")
    install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} -O -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}o')\"
                                  WORKING_DIRECTORY ${directory})")
    list(APPEND ${artifacts} ${directory}/${src_file}
      ${directory}/${src_file}c
      ${directory}/${src_file}o)
  endforeach (src_file)
endmacro(py_compile)

py_compile(${CMAKE_CURRENT_BINARY_DIR} ${pysrc-generated} CPROTON_ARTIFACTS)
py_compile(${CMAKE_CURRENT_SOURCE_DIR} "${pysrc}" PROTON_ARTIFACTS)

find_program(EPYDOC_EXE epydoc)
mark_as_advanced (EPYDOC_EXE)
if (EPYDOC_EXE)
   foreach (py_src_doc ${pysrc})
     list(APPEND PY_DOC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${py_src_doc}")
   endforeach(py_src_doc)
   add_custom_target(docs-py COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../env.py --
     PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}
     ${EPYDOC_EXE} -v --no-private --html -o ${CMAKE_CURRENT_BINARY_DIR}/html
     ${PY_DOC_FILES}
     DEPENDS ${SWIG_MODULE_${cproton}_REAL_NAME})
   add_dependencies(docs docs-py)
   install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/"
           DESTINATION "${PROTON_SHARE}/docs/api-py"
           COMPONENT documentation
           OPTIONAL)
endif (EPYDOC_EXE)

find_program(SPHINX_EXE sphinx-build)
mark_as_advanced (SPHINX_EXE)
if (SPHINX_EXE)
   add_custom_target(tutorial-py COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../env.py --
     PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}
     ${SPHINX_EXE} -b html ${CMAKE_CURRENT_SOURCE_DIR}/docs ${CMAKE_CURRENT_BINARY_DIR}/tutorial)
   add_dependencies(docs tutorial-py)
   install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tutorial/"
           DESTINATION "${PROTON_SHARE}/docs/tutorial-py"
           COMPONENT documentation
           OPTIONAL)
endif (SPHINX_EXE)

install(FILES ${CPROTON_ARTIFACTS}
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)
install(FILES ${PROTON_ARTIFACTS}
        DESTINATION "${PYTHON_SITEARCH_PACKAGES}/proton/"
        COMPONENT Python)
install(TARGETS ${SWIG_MODULE_cproton_REAL_NAME}
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "html;tutorial")
