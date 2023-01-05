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

set (pysrc
    proton/__init__.py
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
    setup.py
    setup.cfg
    pyproject.toml
    README.rst
    MANIFEST.in
    ext_build.py
    cproton.h
    cproton.py
    docs/conf.py
    docs/index.rst
    docs/overview.rst
    docs/tutorial.rst
    )

# Sphinx documentation
check_python_module("sphinx" SPHINX_MODULE_FOUND)
if (NOT SPHINX_MODULE_FOUND)
    message(STATUS "Sphinx modules not found; doc generation disabled.")
else ()
    add_custom_target(docs-py
        COMMAND ${PN_ENV_SCRIPT} --
        PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:${CMAKE_CURRENT_SOURCE_DIR}
        LD_LIBRARY_PATH="${CMAKE_CURRENT_BINARY_DIR}/c"
        ${Python_EXECUTABLE} -m sphinx "${CMAKE_CURRENT_SOURCE_DIR}/docs" "${CMAKE_CURRENT_BINARY_DIR}/docs")
    add_dependencies(docs docs-py)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/"
            DESTINATION "${PROTON_SHARE}/docs/api-py"
            COMPONENT documentation
            OPTIONAL)
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES docs)
endif ()

if (CHECK_SYSINSTALL_PYTHON)
  execute_process(COMMAND ${Python_EXECUTABLE}
    -c "from sysconfig import get_path; print(get_path('platlib'))"
    OUTPUT_VARIABLE PYTHON_SITEARCH_PACKAGES_DEFAULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if (NOT PYTHON_SITEARCH_PACKAGES)
    set (PYTHON_SITEARCH_PACKAGES ${PYTHON_SITEARCH_PACKAGES_DEFAULT})
  endif()

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

  py_compile(${CMAKE_CURRENT_BINARY_DIR} ${pysrc-generated} CPROTON_ARTIFACTS)
  py_compile(${CMAKE_CURRENT_SOURCE_DIR} "${pysrc}" PROTON_ARTIFACTS)

  install(FILES ${CPROTON_ARTIFACTS}
          DESTINATION ${PYTHON_SITEARCH_PACKAGES}
          COMPONENT Python)
  install(FILES ${PROTON_ARTIFACTS}
          DESTINATION "${PYTHON_SITEARCH_PACKAGES}/proton/"
          COMPONENT Python)
  install(TARGETS ${SWIG_MODULE_cproton_REAL_NAME}
          DESTINATION ${PYTHON_SITEARCH_PACKAGES}
          COMPONENT Python)
endif ()

# Install python examples even without a 'sysinstall' python
install(DIRECTORY examples/
        DESTINATION "${PROTON_SHARE}/examples/python"
        COMPONENT Python
        USE_SOURCE_PERMISSIONS)

#
# Set up the directory 'dist' for building the python native package
# source distribution for Pypi/pip
#

set(py_dist_dir ${CMAKE_CURRENT_BINARY_DIR}/dist)

add_custom_target(py_src_dist ALL)
add_dependencies(py_src_dist generated_c_files ${SWIG_MODULE_cproton_REAL_NAME})

add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_INCLUDE_DIR} "${py_dist_dir}/include"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/core "${py_dist_dir}/src/core"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/compiler "${py_dist_dir}/src/compiler"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/platform "${py_dist_dir}/src/platform"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/ssl "${py_dist_dir}/src/ssl"
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${PN_C_SOURCE_DIR}/sasl "${py_dist_dir}/src/sasl"
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/VERSION.txt "${py_dist_dir}"
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/encodings.h "${py_dist_dir}/src"
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/protocol.h "${py_dist_dir}/src")

foreach(file IN LISTS py_dist_files pysrc)
add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} "${py_dist_dir}/${file}")
endforeach()

foreach(file IN LISTS cproton-generated)
add_custom_command(TARGET py_src_dist
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${file} "${py_dist_dir}/${file}")
endforeach()

# Make python source and binary packages if we have prerequisistes
check_python_module("setuptools" SETUPTOOLS_MODULE_FOUND)
check_python_module("wheel" WHEEL_MODULE_FOUND)
if (SETUPTOOLS_MODULE_FOUND)
  add_custom_target(py_pkg_src ALL
                     WORKING_DIRECTORY dist
                     DEPENDS py_src_dist
		     COMMAND ${PN_ENV_SCRIPT} "SWIG=${SWIG_EXECUTABLE}" --
		      ${Python_EXECUTABLE} setup.py sdist --dist-dir ${CMAKE_CURRENT_BINARY_DIR}/pkgs)
  if (WHEEL_MODULE_FOUND)
    add_custom_target(py_pkg_wheel ALL
                       WORKING_DIRECTORY dist
                       DEPENDS py_pkg_src
                       COMMAND ${Python_EXECUTABLE} setup.py bdist_wheel --dist-dir ${CMAKE_CURRENT_BINARY_DIR}/pkgs)
  endif ()
endif ()

# python test: python/tests/proton-test
set (py_src "${CMAKE_CURRENT_SOURCE_DIR}")
set (py_bin "${CMAKE_CURRENT_BINARY_DIR}")
# These are only needed on Windows due to build differences
set (py_dll "$<$<PLATFORM_ID:Windows>:$<TARGET_FILE_DIR:cproton>>")
set (py_bld "$<$<PLATFORM_ID:Windows>:$<TARGET_FILE_DIR:qpid-proton-core>>")
set (py_tests "${py_src}/tests")
set (tests_py "${py_src}/../tests/py")

set (py_path ${CMAKE_BINARY_DIR}/c/tools ${py_bld} $ENV{PATH})
set (py_pythonpath ${py_tests} ${py_src} ${py_bin} ${py_dll} ${tests_py} $ENV{PYTHONPATH})
to_native_path ("${py_pythonpath}" py_pythonpath)
to_native_path ("${py_path}" py_path)

if (CMAKE_BUILD_TYPE MATCHES "Coverage")
  set (python_coverage_options -m coverage run --parallel-mode)
endif(CMAKE_BUILD_TYPE MATCHES "Coverage")

# Create Python virtual environment to run tests
set(pytest_venv "${py_bin}/pytest_env")
# Have to use a conditional here as you can't use generator expressions in OUTPUT or BYPRODUCTS
if (WIN32)
 set(py_venv_bin "Scripts")
else()
 set(py_venv_bin "bin")
endif()
set(pytest_bin "${pytest_venv}/${py_venv_bin}")
set(pytest_executable "${pytest_bin}/python${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_command(
  OUTPUT ${pytest_venv}/env.txt
  COMMAND ${Python_EXECUTABLE} -m venv ${pytest_venv}
  COMMAND ${pytest_executable} -m pip freeze > ${pytest_venv}/env.txt
  BYPRODUCTS ${pytest_executable}
)

# Install proton in venv
add_custom_target(
  pytest_cproton_install
  WORKING_DIRECTORY ${py_dist_dir}
  COMMAND ${pytest_executable} setup.py develop
  DEPENDS ${pytest_venv}/env.txt
)
add_dependencies(pytest_cproton_install py_src_dist)

add_custom_target(pytest_install ALL)
add_dependencies(pytest_install pytest_cproton_install)

pn_add_test(
  INTERPRETED
  NAME python-test
  PREPEND_ENVIRONMENT
    "PATH=${py_path}"
    "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
  COMMAND ${pytest_executable} ${python_coverage_options} -- "${py_tests}/proton-test")
set_tests_properties(python-test PROPERTIES PASS_REGULAR_EXPRESSION "Totals: .* 0 failed")

set(PYTHON_TEST_COMMAND "-m" "unittest")
pn_add_test(
  INTERPRETED
  NAME python-integration-test
  PREPEND_ENVIRONMENT
    "PATH=${py_path}"
    "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
  COMMAND
    ${pytest_executable}
      ${python_coverage_options}
      ${PYTHON_TEST_COMMAND} discover -v -s "${py_tests}/integration")

check_python_module("flake8" FLAKE_MODULE_FOUND)
if (FLAKE_MODULE_FOUND)
  option(ENABLE_PEP8_TEST "Enable pep8 python testing with flake8" ON)
  if (ENABLE_PEP8_TEST)
    pn_add_test(
      INTERPRETED
      NAME python-pep8-test
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${Python_EXECUTABLE} -m flake8)
  endif ()
endif ()

check_python_module("tox" TOX_MODULE_FOUND)
if (NOT TOX_MODULE_FOUND)
  message(STATUS "The tox tool is not available; skipping the python-tox-tests")
else ()
  option(ENABLE_TOX_TEST "Enable multi-version python testing with TOX" ON)

  set(TOX_ENVLIST "" CACHE STRING "List of python environments for TOX tests" )
  mark_as_advanced(TOX_ENVLIST)

  if (ENABLE_TOX_TEST)
    if (CMAKE_BUILD_TYPE MATCHES "Coverage")
      message(STATUS "Building for coverage analysis; skipping the python-tox-tests")
    else ()
      add_custom_command(TARGET py_src_dist
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini"
        "${py_dist_dir}/tox.ini")
      pn_add_test(
        INTERPRETED
        NAME python-tox-test
        WORKING_DIRECTORY ${py_dist_dir}
        PREPEND_ENVIRONMENT
          "PATH=${py_path}"
          "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
          "TOXENV=${TOX_ENVLIST}"
          "PY_TEST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests"
        COMMAND ${Python_EXECUTABLE} -m tox)
      set_tests_properties(python-tox-test
        PROPERTIES
        PASS_REGULAR_EXPRESSION "Totals: .* ignored, 0 failed"
        FAIL_REGULAR_EXPRESSION "ERROR:.*commands failed")
    endif ()
  endif (ENABLE_TOX_TEST)
endif (NOT TOX_MODULE_FOUND)
