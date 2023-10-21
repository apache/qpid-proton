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
                   COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/VERSION.txt            .
                   COMMAND ${CMAKE_COMMAND} -E touch .timestamp.copied_pysrc
                   DEPENDS generated_c_files ${py_cgen} ${py_csrc} ${py_cinc} ${PROJECT_SOURCE_DIR}/VERSION.txt)

foreach(file IN LISTS py_dist_files pysrc)
  add_custom_command(OUTPUT "${file}"
                     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${file}
                     DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
  list(APPEND pysrc_files "${CMAKE_CURRENT_BINARY_DIR}/${file}")
endforeach()

add_custom_target(pysrc_copied ALL DEPENDS .timestamp.copied_pysrc ${pysrc_files})

add_custom_command(OUTPUT ./tox.ini
                   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini" tox.ini
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tox.ini")

# Make python source and binary packages if we have prerequisites
check_python_module("setuptools" SETUPTOOLS_MODULE_FOUND)
check_python_module("wheel" WHEEL_MODULE_FOUND)
check_python_module("cffi" CFFI_MODULE_FOUND)
if (SETUPTOOLS_MODULE_FOUND)
  set (pydist_cmds sdist)
  if (WHEEL_MODULE_FOUND AND CFFI_MODULE_FOUND)
    set (pydist_cmds sdist bdist_wheel --py-limited-api=cp38)
  endif()
endif()

if (pydist_cmds)
  add_custom_command(OUTPUT .timestamp.dist
    DEPENDS pysrc_copied
    COMMAND ${CMAKE_COMMAND} -E remove -f .timestamp.dist
    COMMAND ${Python_EXECUTABLE} setup.py ${pydist_cmds}
    COMMAND ${CMAKE_COMMAND} -E touch .timestamp.dist)
  add_custom_target(pydist ALL DEPENDS .timestamp.dist)
endif ()

if (BUILD_TESTING)
  add_custom_target(pytest_cffi ALL DEPENDS .timestamp.cproton_ffi)

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

  option(ENABLE_PYTHON_TEST_VENV "Enable python testing within a separate virtual environment." ON)
  if (ENABLE_PYTHON_TEST_VENV)
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
      COMMAND ${pytest_executable} -m pip install --disable-pip-version-check cffi setuptools
      COMMAND ${pytest_executable} -m pip freeze > ${pytest_venv}/env.txt
      BYPRODUCTS ${pytest_executable}
    )

  else()
    set(pytest_executable "${Python_EXECUTABLE}")
    set(pytest_venv "${py_bin}")
    FILE(TOUCH "${pytest_venv}/env.txt")
  endif()

  # Create c code for cffi extension
  add_custom_command(
    OUTPUT .timestamp.cproton_ffi
    COMMAND ${CMAKE_COMMAND} -E remove -f .timestamp.cproton_ffi
    COMMAND ${pytest_executable} ext_build.py
    COMMAND ${CMAKE_COMMAND} -E touch .timestamp.cproton_ffi
    DEPENDS ${pytest_venv}/env.txt pysrc_copied
  )

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
endif(BUILD_TESTING)

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
    endif ()
  endif (ENABLE_TOX_TEST)
endif (NOT TOX_MODULE_FOUND)
