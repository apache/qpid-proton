find_package (PythonLibs)

swig_add_module(cproton python python.i)
include_directories (${PYTHON_INCLUDE_PATH})
swig_link_libraries(cproton qpidproton ${LINK_DEPS} ${PYTHON_LIBRARIES})
