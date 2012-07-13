find_package (PythonLibs)

set(CMAKE_SWIG_FLAGS "-threads")

swig_add_module(cproton python python.i)
include_directories (${PYTHON_INCLUDE_PATH})
swig_link_libraries(cproton ${BINDING_DEPS} ${PYTHON_LIBRARIES})
