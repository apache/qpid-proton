set(CMAKE_SWIG_FLAGS "-threads")

swig_add_module(cproton python python.i)
include_directories (${PYTHON_INCLUDE_PATH})
swig_link_libraries(cproton ${BINDING_DEPS} ${PYTHON_LIBRARIES})

find_package(PythonInterp REQUIRED)

execute_process(COMMAND ${PYTHON_EXECUTABLE}
                -c "from distutils.sysconfig import get_python_lib; print get_python_lib(True, prefix='')"
                OUTPUT_VARIABLE PYTHON_SITEARCH_PACKAGES
                OUTPUT_STRIP_TRAILING_WHITESPACE)

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
   add_custom_target(docs-py COMMAND ${EPYDOC_EXE} --no-private --html
                     -o ${CMAKE_CURRENT_BINARY_DIR}/html
                     ${CMAKE_CURRENT_SOURCE_DIR}/proton.py)
   add_dependencies(docs docs-py)
   install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
           DESTINATION ${PROTON_SHARE}/docs/api-py
           COMPONENT documentation
           OPTIONAL)
endif (EPYDOC_EXE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cproton.py
              ${CMAKE_CURRENT_BINARY_DIR}/cproton.pyc
              ${CMAKE_CURRENT_BINARY_DIR}/cproton.pyo
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.py
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.pyc
              ${CMAKE_CURRENT_SOURCE_DIR}/proton.pyo
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/_cproton.so
        RENAME _cproton.so
        DESTINATION ${PYTHON_SITEARCH_PACKAGES}
        COMPONENT Python)
