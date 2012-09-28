cmake_minimum_required (VERSION 2.6)

project (Proton C)

set (PN_VERSION_MAJOR 0)
set (PN_VERSION_MINOR 1)

set (PREFIX ${CMAKE_INSTALL_PREFIX})
set (EXEC_PREFIX ${CMAKE_INSTALL_PREFIX})
set (LIBDIR ${CMAKE_INSTALL_PREFIX}/lib CACHE STRING "Library directory")
set (INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)
set (PN_VERSION "${PN_VERSION_MAJOR}.${PN_VERSION_MINOR}")

include(CheckIncludeFile)
CHECK_INCLUDE_FILE(openssl/ssl.h HAVE_OPENSSL_H)

# Set the default SSL/TLS implementation
set(ssl_impl, NONE)
if (HAVE_OPENSSL_H)
   set(ssl_impl openssl)
endif(HAVE_OPENSSL_H)
set(SSL_IMPL ${ssl_impl} CACHE STRING "Library to use for SSL/TLS support. Valid values: 'none','openssl'")

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

# Link in openssl if present
if (SSL_IMPL STREQUAL openssl)
  set (pn_driver_ssl_impl src/ssl/openssl.c)
  set (SSL_LIB ssl crypto)
else (SSL_IMPL STREQUAL openssl)
  set (pn_driver_ssl_impl src/ssl/ssl_stub.c)
endif (SSL_IMPL STREQUAL openssl)

find_package(SWIG)
if (SWIG_FOUND)
  add_subdirectory(bindings)
endif (SWIG_FOUND)

add_subdirectory(docs/api)

# Should really be finding the uuid library appropriate for the platform
# in lieu of doing this set the library name directly.
set (UUID_LIB uuid)

set (qpid-proton-platform
  src/driver.c
  ${pn_driver_ssl_impl}
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

  src/messenger.c

  ${qpid-proton-platform}

  ${PROJECT_BINARY_DIR}/encodings.h
  ${PROJECT_BINARY_DIR}/protocol.h
)
target_link_libraries (qpid-proton ${UUID_LIB} ${SSL_LIB})

add_executable (proton src/proton.c)
target_link_libraries (proton qpid-proton)

add_executable (proton-dump src/proton-dump.c)
target_link_libraries (proton-dump qpid-proton)

set_target_properties (
  qpid-proton proton proton-dump
  PROPERTIES
  COMPILE_FLAGS "-Wall -Werror -pedantic-errors -std=c99 -g -Iinclude -fPIC"
)

# Install executables and libraries
install (TARGETS proton proton-dump qpid-proton
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION ${LIBDIR})

# Install header files
file(GLOB headers "include/proton/*.[hi]")
install (FILES ${headers} DESTINATION include/proton)

# Pkg config file

configure_file(${PROJECT_SOURCE_DIR}/src/libqpid-proton.pc.in
         ${PROJECT_BINARY_DIR}/libqpid-proton.pc @ONLY)
install (FILES ${PROJECT_BINARY_DIR}/libqpid-proton.pc
         DESTINATION ${LIBDIR}/pkgconfig)

