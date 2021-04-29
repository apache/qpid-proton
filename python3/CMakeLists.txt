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

include_directories (${PN_C_INCLUDE_DIR} ${Python_INCLUDE_DIRS})

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/_proton_core.c
        COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/generate_binding_code.py" "${CMAKE_CURRENT_BINARY_DIR}/_proton_core.c"
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/generate_binding_code.py)

if (WIN32)
  set(python_suffix .pyd)
else ()
  set(python_suffix .so)
endif()

add_library(_proton_core SHARED ${CMAKE_CURRENT_BINARY_DIR}/_proton_core.c)
target_link_directories(_proton_core PRIVATE ${Python_LIBRARY_DIRS})  # TODO: this will not work on older cmake
target_link_libraries(_proton_core ${BINDING_DEPS} ${Python_LIBRARIES} -lm)
set_target_properties(_proton_core
    PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_NAME "_proton_core.abi3"
        OUTPUT_NAME "_proton_core" # on Windows
        SUFFIX "${python_suffix}"
        COMPILE_FLAGS "${C_STANDARD_FLAGS}" # ${COMPILE_WARNING_FLAGS} breaks on -Wpedantic
        LINK_FLAGS "${CATCH_UNDEFINED}")

if (CHECK_SYSINSTALL_PYTHON)
  execute_process(COMMAND ${Python_EXECUTABLE}
    -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(True))"
    OUTPUT_VARIABLE PYTHON_SITEARCH_PACKAGES_DEFAULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
else ()
  set (PYTHON_SITEARCH_PACKAGES_DEFAULT ${BINDINGS_DIR}/python3)
endif ()

if (NOT PYTHON_SITEARCH_PACKAGES)
  set (PYTHON_SITEARCH_PACKAGES ${PYTHON_SITEARCH_PACKAGES_DEFAULT})
endif()

set (pysrc
    proton/__init__.py
#    proton/_compat.py
#    proton/_common.py
#    proton/_condition.py
#    proton/_data.py
#    proton/_delivery.py
#    proton/_endpoints.py
#    proton/_events.py
#    proton/_exceptions.py
#    proton/_io.py
#    proton/_message.py
#    proton/_tracing.py
#    proton/_transport.py
#    proton/_url.py
#    proton/_wrapper.py
#
#    proton/handlers.py
#    proton/reactor.py
#    proton/tracing.py
#    proton/utils.py
#
#    proton/_handlers.py
#    proton/_reactor.py
#    proton/_selectable.py
#    proton/_utils.py
    )

macro (py_compile directory files artifacts)
  foreach (src_file ${files})
    install(CODE "execute_process(COMMAND \"${Python_EXECUTABLE}\" -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}c')\"
                                  WORKING_DIRECTORY ${directory})")
    install(CODE "execute_process(COMMAND \"${Python_EXECUTABLE}\" -O -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}o')\"
                                  WORKING_DIRECTORY ${directory})")
    list(APPEND ${artifacts} ${directory}/${src_file}
      ${directory}/${src_file}c
      ${directory}/${src_file}o)
  endforeach (src_file)
endmacro(py_compile)

py_compile(${CMAKE_CURRENT_SOURCE_DIR} "${pysrc}" PROTON_ARTIFACTS)

install(FILES ${PROTON_ARTIFACTS}
        DESTINATION "${PYTHON_SITEARCH_PACKAGES}/proton/"
        COMPONENT Python)
install(TARGETS _proton_core
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)

# python test: python3/tests/proton-test
set (py_src "${CMAKE_CURRENT_SOURCE_DIR}")
set (py_bin "${CMAKE_CURRENT_BINARY_DIR}")
set (py_dll "$<TARGET_FILE_DIR:_proton_core>")
set (py_bld "$<TARGET_FILE_DIR:qpid-proton-core>") # For windows
set (py_tests "${py_src}/tests")
set (tests_py "${py_src}/../tests/py")

set (py_path ${CMAKE_BINARY_DIR}/c/tools ${py_bld} $ENV{PATH})
set (py_pythonpath ${py_tests} ${py_src} ${py_bin} ${py_dll} ${tests_py} $ENV{PYTHONPATH})
to_native_path ("${py_pythonpath}" py_pythonpath)
to_native_path ("${py_path}" py_path)

if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  set (python_coverage_options -m coverage run --parallel-mode)
endif(CMAKE_BUILD_TYPE MATCHES "Coverage")

set(PYTHON_TEST_COMMAND "-m" "unittest" "discover" "-v" "-s")
pn_add_test(
  INTERPRETED
  NAME python3-test
  PREPEND_ENVIRONMENT
    "PATH=${py_path}"
    "PYTHONPATH=${py_pythonpath}"
    "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
  COMMAND ${Python_EXECUTABLE} ${python_coverage_options} ${PYTHON_TEST_COMMAND} "${py_tests}/python3-test")
set_tests_properties(python3-test PROPERTIES PASS_REGULAR_EXPRESSION "Ran .+ tests in .*\n\nOK")
