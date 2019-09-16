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

set(CMAKE_SWIG_FLAGS "-threads" "-DUINTPTR_SIZE=${CMAKE_SIZEOF_VOID_P}")

include_directories (${PN_C_INCLUDE_DIR} ${PYTHON_INCLUDE_PATH})

set_source_files_properties(cproton.i PROPERTIES CPLUSPLUS NO)

# Suppress warnings in swig generated code.
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

# Set Compiler extra flags for Solaris when using SunStudio
if (CMAKE_C_COMPILER_ID STREQUAL "SunPro")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSWIGEXPORT=__global")
endif ()


list(APPEND SWIG_MODULE_cproton_EXTRA_DEPS
    ${CMAKE_SOURCE_DIR}/c/include/proton/cproton.i
    ${PROTON_HEADERS}
)

swig_add_library(cproton LANGUAGE python SOURCES cproton.i)
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
    proton/_compat.py
    proton/_common.py
    proton/_condition.py
    proton/_data.py
    proton/_delivery.py
    proton/_endpoints.py
    proton/_events.py
    proton/_exceptions.py
    proton/_io.py
    proton/_message.py
    proton/_tracing.py
    proton/_transport.py
    proton/_url.py
    proton/_wrapper.py

    proton/handlers.py
    proton/reactor.py
    proton/tracing.py
    proton/utils.py

    proton/_handlers.py
    proton/_reactor.py
    proton/_selectable.py
    proton/_utils.py
    )
# extra files included in the source distribution
set(py_dist_files
    cproton.i
    MANIFEST.in
    setuputils/__init__.py
    setuputils/log.py
    setuputils/misc.py
    setuputils/PYZMQ_LICENSE.BSD
    docs/conf.py
    docs/index.rst
    docs/overview.rst
    docs/tutorial.rst
    )

macro (py_compile directory files artifacts)
  foreach (src_file ${files})
    install(CODE "execute_process(COMMAND \"${PYTHON_EXECUTABLE}\" -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}c')\"
                                  WORKING_DIRECTORY ${directory})")
    install(CODE "execute_process(COMMAND \"${PYTHON_EXECUTABLE}\" -O -c \"import py_compile; py_compile.compile('${src_file}', cfile='${src_file}o')\"
                                  WORKING_DIRECTORY ${directory})")
    list(APPEND ${artifacts} ${directory}/${src_file}
      ${directory}/${src_file}c
      ${directory}/${src_file}o)
  endforeach (src_file)
endmacro(py_compile)

py_compile(${CMAKE_CURRENT_BINARY_DIR} ${pysrc-generated} CPROTON_ARTIFACTS)
py_compile(${CMAKE_CURRENT_SOURCE_DIR} "${pysrc}" PROTON_ARTIFACTS)

# Sphinx documentation
check_python_module("sphinx" SPHINX_MODULE_FOUND)
check_python_module("sphinx_automodapi" SPHINX_AUTOMODAPI_MODULE_FOUND)
if (NOT SPHINX_MODULE_FOUND OR NOT SPHINX_AUTOMODAPI_MODULE_FOUND)
    message(STATUS "Sphinx modules not found; doc generation disabled.")
else ()
    add_custom_target(docs-py
        COMMAND ${PN_ENV_SCRIPT} --
        PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}
        LD_LIBRARY_PATH="${CMAKE_CURRENT_BINARY_DIR}/c"
        ${PYTHON_EXECUTABLE} -m sphinx "${CMAKE_CURRENT_SOURCE_DIR}/docs" "${CMAKE_CURRENT_BINARY_DIR}/docs")
    add_dependencies(docs docs-py)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/"
            DESTINATION "${PROTON_SHARE}/docs/api-py"
            COMPONENT documentation
            OPTIONAL)
endif ()

install(FILES ${CPROTON_ARTIFACTS}
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)
install(FILES ${PROTON_ARTIFACTS}
        DESTINATION "${PYTHON_SITEARCH_PACKAGES}/proton/"
        COMPONENT Python)
install(TARGETS ${SWIG_MODULE_cproton_REAL_NAME}
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)
install(DIRECTORY examples/
        DESTINATION "${PROTON_SHARE}/examples/python"
        COMPONENT Python
        USE_SOURCE_PERMISSIONS)

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES docs)

#
# Set up the directory 'dist' for building the python native package
# source distribution for Pypi/pip
#

set(py_dist_dir ${CMAKE_CURRENT_BINARY_DIR}/dist)

add_custom_target(py_src_dist ALL)
add_dependencies(py_src_dist generated_c_files)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.rst.in
               ${CMAKE_CURRENT_BINARY_DIR}/README.rst
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in
               ${CMAKE_CURRENT_BINARY_DIR}/setup.py
)

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_INCLUDE_DIR} "${py_dist_dir}/include")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/core "${py_dist_dir}/src/core")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/compiler "${py_dist_dir}/src/compiler")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/platform "${py_dist_dir}/src/platform")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/ssl "${py_dist_dir}/src/ssl")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/sasl "${py_dist_dir}/src/sasl")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/encodings.h "${py_dist_dir}/src")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/protocol.h "${py_dist_dir}/src")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/README.rst "${py_dist_dir}")

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/setup.py "${py_dist_dir}")

foreach(file IN LISTS py_dist_files pysrc)
add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} "${py_dist_dir}/${file}")
endforeach()

# Make python source and binary packages if we have prerequisistes
check_python_module("setuptools" SETUPTOOLS_MODULE_FOUND)
check_python_module("wheel" WHEEL_MODULE_FOUND)
if (SETUPTOOLS_MODULE_FOUND)
  add_custom_target(py_pkg_src ALL
                     WORKING_DIRECTORY dist
                     DEPENDS py_src_dist
                     COMMAND ${PYTHON_EXECUTABLE} setup.py sdist --dist-dir ${CMAKE_CURRENT_BINARY_DIR}/pkgs)
  if (WHEEL_MODULE_FOUND)
    add_custom_target(py_pkg_wheel ALL
                       WORKING_DIRECTORY dist
                       DEPENDS py_pkg_src
                       COMMAND ${PYTHON_EXECUTABLE} setup.py bdist_wheel --dist-dir ${CMAKE_CURRENT_BINARY_DIR}/pkgs)
  endif ()
endif ()

# python test: python/tests/proton-test
set (py_src "${CMAKE_CURRENT_SOURCE_DIR}")
set (py_bin "${CMAKE_CURRENT_BINARY_DIR}")
set (py_dll "$<TARGET_FILE_DIR:_cproton>")
set (py_bld "$<TARGET_FILE_DIR:qpid-proton-core>") # For windows
set (py_tests "${py_src}/tests")

set (py_path ${CMAKE_BINARY_DIR}/c/tools ${py_bld} $ENV{PATH})
set (py_pythonpath ${py_tests} ${py_src} ${py_bin} ${py_dll} $ENV{PYTHONPATH})
to_native_path ("${py_pythonpath}" py_pythonpath)
to_native_path ("${py_path}" py_path)

if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  set (python_coverage_options -m coverage run)
endif(CMAKE_BUILD_TYPE MATCHES "Coverage")

add_test (NAME python-test
  COMMAND ${PN_ENV_SCRIPT}
  "PATH=${py_path}" "PYTHONPATH=${py_pythonpath}"
  "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
  ${TEST_ENV}
  ${TEST_WRAP_PREFIX_CMD} ${PYTHON_EXECUTABLE} -- ${python_coverage_options} "${py_tests}/proton-test")
set_tests_properties(python-test PROPERTIES PASS_REGULAR_EXPRESSION "Totals: .* 0 failed")

check_python_module("tox" TOX_MODULE_FOUND)
if (NOT TOX_MODULE_FOUND)
  message(STATUS "The tox tool is not available; skipping the python-tox-tests")
else ()
  option(ENABLE_TOX_TEST "Enable multi-version python testing with TOX" ON)

  set(tox_default "py26,py27,py35,py36,py37,py38")
  set(TOX_ENVLIST "" CACHE STRING "List of python environments for TOX tests" )
  mark_as_advanced(TOX_ENVLIST)

  if (NOT TOX_ENVLIST)
    set (TOX_ENVLIST ${tox_default})
  endif()
  if (ENABLE_TOX_TEST)
    if (CMAKE_BUILD_TYPE MATCHES "Coverage")
      message(STATUS "Building for coverage analysis; skipping the python-tox-tests")
    else ()
      configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini.in"
        "${CMAKE_CURRENT_BINARY_DIR}/tox.ini")
      add_test (NAME python-tox-test
        COMMAND ${PN_ENV_SCRIPT}
        "PATH=${py_path}"
        "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
        "SWIG=${SWIG_EXECUTABLE}"
        ${TEST_ENV} --
        ${TEST_WRAP_PREFIX_CMD} ${PYTHON_EXECUTABLE} -m tox)
      set_tests_properties(python-tox-test
        PROPERTIES
        PASS_REGULAR_EXPRESSION "Totals: .* ignored, 0 failed"
        FAIL_REGULAR_EXPRESSION "ERROR:[ ]+py[0-9]*: commands failed")
    endif ()
  endif (ENABLE_TOX_TEST)
endif (NOT TOX_MODULE_FOUND)
