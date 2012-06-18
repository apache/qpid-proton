cmake_minimum_required (VERSION 2.6)

project (Proton)

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 1)

configure_file (
  "${PROJECT_SOURCE_DIR}/pn_config.h.in"
  "${PROJECT_BINARY_DIR}/pn_config.h"
)

include_directories ("${PROJECT_BINARY_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/include")

add_custom_command (
  OUTPUT ${PROJECT_BINARY_DIR}/encodings.h
  COMMAND PYTHONPATH=${PROJECT_SOURCE_DIR} python ${PROJECT_SOURCE_DIR}/src/codec/encodings.h.py > ${PROJECT_BINARY_DIR}/encodings.h
  DEPENDS ${PROJECT_SOURCE_DIR}/src/codec/encodings.h.py
)

add_custom_command (
  OUTPUT ${PROJECT_BINARY_DIR}/protocol.h
  COMMAND PYTHONPATH=${PROJECT_SOURCE_DIR} python ${PROJECT_SOURCE_DIR}/src/protocol.h.py > ${PROJECT_BINARY_DIR}/protocol.h
  DEPENDS ${PROJECT_SOURCE_DIR}/src/protocol.h.py
)

add_library (
  qpidproton SHARED

  src/util.c
  src/driver.c
  src/error.c
  src/buffer.c
  src/parser.c
  src/scanner.c
  src/types.c

  src/framing/framing.c

  src/codec/codec.c

  src/dispatcher/dispatcher.c
  src/engine/engine.c
  src/message/message.c
  src/sasl/sasl.c

  ${PROJECT_BINARY_DIR}/encodings.h
  ${PROJECT_BINARY_DIR}/protocol.h

  src/messenger.c
)

include(FindSWIG)
if (SWIG_FOUND)
  add_subdirectory(bindings)
endif (SWIG_FOUND)

add_executable (proton src/proton.c)
target_link_libraries (proton qpidproton)

add_subdirectory(docs/api)

set_target_properties (
  qpidproton proton
  PROPERTIES
  COMPILE_FLAGS "-Wall -Werror -pedantic-errors -std=c99 -g -Iinclude -fPIC"
)

install (TARGETS proton qpidproton
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib)
