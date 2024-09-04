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
    proton/_handler.py
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
    pyproject.toml
    README.rst
    MANIFEST.in
    ext_build.py
    ext_build_devtree.py
    ext_build_unbundled.py
    cproton.h
    cproton_ext.c
    cproton.py
    docs/conf.py
    docs/index.rst
    docs/overview.rst
    docs/tutorial.rst
    proton/py.typed
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

install(DIRECTORY examples/
        DESTINATION "${PROTON_SHARE}/examples/python"
        COMPONENT Python
        USE_SOURCE_PERMISSIONS)

#
# Set up the directory for building the python native package
# source distribution for Pypi/pip
#

set(py_csrc_dir ${PROJECT_SOURCE_DIR}/c/src)
set(py_cinc_dir ${PROJECT_SOURCE_DIR}/c/include)

file(GLOB_RECURSE py_csrc LIST_DIRECTORIES no
  "${py_csrc_dir}/core/*.[ch]"
  "${py_csrc_dir}/compiler/*.[ch]"
  "${py_csrc_dir}/platform/*.[ch]"
  "${py_csrc_dir}/ssl/*.[ch]"
  "${py_csrc_dir}/ssl/*.cpp"
  "${py_csrc_dir}/sasl/*.[ch]" )

file(GLOB_RECURSE py_cinc LIST_DIRECTORIES no
  "${py_cinc_dir}/proton/*.h")

set(py_cgen
  ${PN_C_INCLUDE_DIR}/proton/version.h
  ${PN_C_SOURCE_DIR}/encodings.h
  ${PN_C_SOURCE_DIR}/protocol.h
  ${PN_C_SOURCE_DIR}/core/frame_generators.c
  ${PN_C_SOURCE_DIR}/core/frame_generators.h
  ${PN_C_SOURCE_DIR}/core/frame_consumers.c
  ${PN_C_SOURCE_DIR}/core/frame_consumers.h)

add_custom_command(OUTPUT .timestamp.copied_pysrc
                   COMMAND ${CMAKE_COMMAND} -E remove -f .timestamp.copied_pysrc
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_cinc_dir}          include
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_csrc_dir}/core     src/core
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_csrc_dir}/compiler src/compiler
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_csrc_dir}/platform src/platform
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_csrc_dir}/ssl      src/ssl
                   COMMAND ${CMAKE_COMMAND} -E copy_directory ${py_csrc_dir}/sasl     src/sasl
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_INCLUDE_DIR}/proton/version.h       include/proton
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/encodings.h             src
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/protocol.h              src
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/core/frame_generators.c src/core
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/core/frame_generators.h src/core
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/core/frame_consumers.c  src/core
                   COMMAND ${CMAKE_COMMAND} -E copy ${PN_C_SOURCE_DIR}/core/frame_consumers.h  src/core
                   COMMAND ${CMAKE_COMMAND} -E touch .timestamp.copied_pysrc
                   DEPENDS generated_c_files ${py_cgen} ${py_csrc} ${py_cinc} ${PROJECT_SOURCE_DIR}/VERSION.txt)

foreach(file IN LISTS py_dist_files pysrc)
  add_custom_command(OUTPUT "${file}"
                     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${file}
                     DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
  list(APPEND pysrc_files "${CMAKE_CURRENT_BINARY_DIR}/${file}")
endforeach()

add_custom_command(OUTPUT VERSION.txt
                  COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/VERSION.txt .
                  DEPENDS ${PROJECT_SOURCE_DIR}/VERSION.txt)

add_custom_target(pysrc_copied DEPENDS ${pysrc_files} VERSION.txt)
add_custom_target(pypkg_src_copied ALL DEPENDS pysrc_copied .timestamp.copied_pysrc)

add_custom_command(OUTPUT ./tox.ini
                   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini" tox.ini
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini")

option(ENABLE_PYTHON_ISOLATED "Enable python building/testing with isolated environments." ON)
option(BUILD_PYTHON_UNBUNDLED_PKG "Build Python package without bundling qpid-proton-core library" Off)

# Make python source and binary packages if we have prerequisites
check_python_module("build" BUILD_MODULE_FOUND)

if (ENABLE_PYTHON_ISOLATED)
  set (pypkgbuildoption "")
else ()
  set (pypkgbuildoption "-n")
endif ()

check_python_module("setuptools" SETUPTOOLS_MODULE_FOUND)
check_python_module("wheel" WHEEL_MODULE_FOUND)
check_python_module("cffi" CFFI_MODULE_FOUND)

if (BUILD_MODULE_FOUND AND
    (ENABLE_PYTHON_ISOLATED OR (SETUPTOOLS_MODULE_FOUND AND WHEEL_MODULE_FOUND AND CFFI_MODULE_FOUND)))
  if (BUILD_PYTHON_UNBUNDLED_PKG)
    add_custom_command(OUTPUT .timestamp.dist
      DEPENDS pypkg_src_copied
      COMMAND ${CMAKE_COMMAND} -E remove -f .timestamp.dist
      COMMAND ${CMAKE_COMMAND} -E env
        QPID_PYTHON_UNBUNDLING=unbundled
        ${Python_EXECUTABLE} -m build ${pypkgbuildoption}
      COMMAND ${CMAKE_COMMAND} -E touch .timestamp.dist)
  else ()
    add_custom_command(OUTPUT .timestamp.dist
      DEPENDS pypkg_src_copied
      COMMAND ${CMAKE_COMMAND} -E remove -f .timestamp.dist
      COMMAND ${Python_EXECUTABLE} -m build ${pypkgbuildoption}
      COMMAND ${CMAKE_COMMAND} -E touch .timestamp.dist)
  endif ()
  add_custom_target(pydist ALL DEPENDS .timestamp.dist)
endif ()

if (BUILD_TESTING)
  # python test: python/tests/proton-test
  set (py_src "${CMAKE_CURRENT_SOURCE_DIR}")
  set (py_bin "${CMAKE_CURRENT_BINARY_DIR}")
  # These are only needed on Windows due to build differences
  set (py_bld "$<$<PLATFORM_ID:Windows>:$<TARGET_FILE_DIR:qpid-proton-core>>")
  set (py_tests "${py_src}/tests")
  set (tests_py "${py_src}/../tests/py")

  set (py_path $<TARGET_FILE_DIR:msgr-recv> ${py_bld} $ENV{PATH})
  set (py_pythonpath ${py_tests} ${py_src} ${py_bin} ${tests_py} $ENV{PYTHONPATH})
  to_native_path ("${py_pythonpath}" py_pythonpath)
  to_native_path ("${py_path}" py_path)

  if (CMAKE_BUILD_TYPE MATCHES "Coverage")
    set (python_coverage_options -m coverage run --parallel-mode)
  endif(CMAKE_BUILD_TYPE MATCHES "Coverage")

  if (ENABLE_PYTHON_ISOLATED)
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
      OUTPUT .timestamp.test_env
      COMMAND ${Python_EXECUTABLE} -m venv ${pytest_venv}
      COMMAND ${pytest_executable} -m pip install --disable-pip-version-check cffi
      COMMAND ${CMAKE_COMMAND} -E env
        "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "QPID_PROTON_CORE_TARGET_DIR=$<TARGET_FILE_DIR:qpid-proton-core>"
        "QPID_PYTHON_UNBUNDLING=devtree"
        ${pytest_executable} -m pip install -e .
      COMMAND ${pytest_executable} -m pip freeze > ${pytest_venv}/env.txt
      COMMAND ${CMAKE_COMMAND} -E touch .timestamp.test_env
      BYPRODUCTS ${pytest_executable}
    )
  add_custom_target(pytest_cffi ALL DEPENDS .timestamp.test_env)
  elseif(CFFI_MODULE_FOUND)
    set(pytest_executable "${Python_EXECUTABLE}")
    set(pytest_venv "${py_bin}")
    add_custom_command(
      OUTPUT .timestamp.test_env
      COMMAND ${CMAKE_COMMAND} -E remove -f ${pytest_venv}/env.txt
      COMMAND ${CMAKE_COMMAND} -E env
        "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "QPID_PROTON_CORE_TARGET_DIR=$<TARGET_FILE_DIR:qpid-proton-core>"
        ${pytest_executable} ext_build_devtree.py
      COMMAND ${CMAKE_COMMAND} -E touch .timestamp.test_env
      DEPENDS pysrc_copied qpid-proton-core
    )
  add_custom_target(pytest_cffi ALL DEPENDS .timestamp.test_env)
  endif()



  if (TARGET pytest_cffi)
    # If we are on windows copy the qpid-proton-core dll to the test directory so we can find it easily
    if (WIN32)
      add_custom_command(
        OUTPUT .timestamp.test_env
        APPEND
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:qpid-proton-core>" .
      )
    endif()

    pn_add_test(
      INTERPRETED
      NAME python-test
      PREPEND_ENVIRONMENT
        "PATH=${py_path}"
        "PYTHONPATH=."
        "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
      COMMAND ${pytest_executable} ${python_coverage_options} -- "${py_tests}/proton-test")
    set_tests_properties(python-test PROPERTIES PASS_REGULAR_EXPRESSION "Totals: .* 0 failed")

    set(PYTHON_TEST_COMMAND "-m" "unittest")
    pn_add_test(
      INTERPRETED
      NAME python-integration-test
      PREPEND_ENVIRONMENT
        "PATH=${py_path}"
        "PYTHONPATH=.:${py_pythonpath}"
        "SASLPASSWD=${CyrusSASL_Saslpasswd_EXECUTABLE}"
      COMMAND
        ${pytest_executable}
          ${python_coverage_options}
          ${PYTHON_TEST_COMMAND} discover -v -s "${py_tests}/integration")
  endif()

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
  option(ENABLE_TOX_TEST "Enable multi-version python testing with TOX" ${TOX_MODULE_FOUND})
  set (DO_TOX_TEST ${ENABLE_TOX_TEST})

  if (DO_TOX_TEST AND (NOT TOX_MODULE_FOUND OR NOT TARGET pydist))
    message(STATUS "The tox prerequisites not available; skipping the python-tox-tests")
    set (DO_TOX_TEST Off)
  endif ()

  if (DO_TOX_TEST AND CMAKE_BUILD_TYPE MATCHES "Coverage")
    message(STATUS "Building for coverage analysis; skipping the python-tox-tests")
    set (DO_TOX_TEST Off)
  endif ()

  if (DO_TOX_TEST)
    set(TOX_ENVLIST "" CACHE STRING "List of python environments for TOX tests" )
    mark_as_advanced(TOX_ENVLIST)

    add_custom_target(pytest_tox ALL DEPENDS pydist tox.ini)
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
      REQUIRED_FILES tox.ini
      PASS_REGULAR_EXPRESSION "Totals: .* ignored, 0 failed"
      FAIL_REGULAR_EXPRESSION "ERROR:.*commands failed")
  endif (DO_TOX_TEST)

endif(BUILD_TESTING)
