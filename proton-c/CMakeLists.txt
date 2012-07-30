cmake_minimum_required (VERSION 2.6)

project (Proton C)

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 1)
set (LINK_DEPS uuid)

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
  qpid-proton SHARED

  src/util.c
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
)

add_library (
  qpid-proton-posix SHARED

  src/driver.c
  src/messenger.c
)

find_package(SWIG)
if (SWIG_FOUND)
  add_subdirectory(bindings)
endif (SWIG_FOUND)

add_executable (proton src/proton.c)
target_link_libraries (proton qpid-proton qpid-proton-posix ${LINK_DEPS})

add_executable (proton-dump src/proton-dump.c)
target_link_libraries (proton-dump qpid-proton)

add_subdirectory(docs/api)

set_target_properties (
  qpid-proton qpid-proton-posix proton proton-dump
  PROPERTIES
  COMPILE_FLAGS "-Wall -Werror -pedantic-errors -std=c99 -g -Iinclude -fPIC"
)

install (TARGETS proton proton-dump qpid-proton qpid-proton-posix
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib)
