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

include (versions.cmake)

if(WIN32 AND NOT CYGWIN)
  # linking against Windows native libraries, including mingw
  set (PN_WINAPI TRUE)
endif(WIN32 AND NOT CYGWIN)

set(ssl_impl, none)
if(PN_WINAPI)
  set(ssl_impl schannel)
  set(ssl_providers "'none','schannel','openssl'")
else(PN_WINAPI)
  if (OPENSSL_FOUND AND Threads_FOUND)
    set(ssl_impl openssl)
  endif ()
  set(ssl_providers "'none','openssl'")
endif(PN_WINAPI)
set(SSL_IMPL ${ssl_impl} CACHE STRING "Library to use for SSL/TLS support. Valid values: ${ssl_providers}")

set(sasl_providers cyrus none)
if (CyrusSASL_FOUND AND Threads_FOUND)
  set (sasl_impl cyrus)
else ()
  set (sasl_impl none)
endif ()
set(SASL_IMPL ${sasl_impl} CACHE STRING "Library to use for SASL support. Valid values: ${sasl_providers}")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/include/proton/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/proton/version.h"
  )

file (GLOB headers "include/proton/*.[hi]")

foreach (sfile ${headers})
  file (RELATIVE_PATH rfile ${CMAKE_CURRENT_SOURCE_DIR} ${sfile})
  configure_file (${sfile} ${CMAKE_CURRENT_BINARY_DIR}/${rfile} COPYONLY)
endforeach ()

include_directories (
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ${PN_C_INCLUDE_DIR}
  "${CMAKE_CURRENT_BINARY_DIR}/src"
  )

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  COMMAND ${PN_ENV_SCRIPT} PYTHONPATH=${CMAKE_SOURCE_DIR}/tools/python ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/encodings.h.py > ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/encodings.h.py
  )

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  COMMAND ${PN_ENV_SCRIPT} PYTHONPATH=${CMAKE_SOURCE_DIR}/tools/python ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol.h.py > ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol.h.py
  )

add_custom_target(
  generated_c_files
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  )

file (GLOB_RECURSE source_files "src/*.[ch]")

foreach (sfile ${source_files})
  file (RELATIVE_PATH rfile ${CMAKE_CURRENT_SOURCE_DIR} ${sfile})
  configure_file (${sfile} ${CMAKE_CURRENT_BINARY_DIR}/${rfile} COPYONLY)
endforeach ()

# Select IO impl
if(PN_WINAPI)
  set (pn_io_impl src/reactor/io/windows/io.c src/reactor/io/windows/iocp.c src/reactor/io/windows/write_pipeline.c)
  set (pn_selector_impl src/reactor/io/windows/selector.c)
else(PN_WINAPI)
  set (pn_io_impl src/reactor/io/posix/io.c)
  set (pn_selector_impl src/reactor/io/posix/selector.c)
endif(PN_WINAPI)

# Link in SASL if present
if (SASL_IMPL STREQUAL cyrus)
  set(pn_sasl_impl src/sasl/sasl.c src/sasl/default_sasl.c src/sasl/cyrus_sasl.c)
  set(SASL_LIB CyrusSASL::CyrusSASL Threads::Threads)
elseif (SASL_IMPL STREQUAL none)
  set(pn_sasl_impl src/sasl/sasl.c src/sasl/default_sasl.c src/sasl/cyrus_stub.c)
endif ()

# Set Compiler extra flags for Solaris when using SunStudio
if(CMAKE_CXX_COMPILER_ID STREQUAL "SunPro" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mt" )
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "SunPro" )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mt")
endif()

# Link in openssl if present
if (SSL_IMPL STREQUAL openssl)
  set (pn_ssl_impl src/ssl/openssl.c)
  set (SSL_LIB OpenSSL::SSL Threads::Threads)
elseif (SSL_IMPL STREQUAL schannel)
  set (pn_ssl_impl src/ssl/schannel.c)
  set (SSL_LIB Crypt32.lib Secur32.lib)
else ()
  set (pn_ssl_impl src/ssl/ssl_stub.c)
endif ()

# First check whether we get clock_gettime without any special library linked
CHECK_SYMBOL_EXISTS(clock_gettime "time.h" CLOCK_GETTIME_IN_LIBC)
if (CLOCK_GETTIME_IN_LIBC)
  list(APPEND PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME")
else (CLOCK_GETTIME_IN_LIBC)
  CHECK_LIBRARY_EXISTS (rt clock_gettime "" CLOCK_GETTIME_IN_RT)
  if (CLOCK_GETTIME_IN_RT)
    set (TIME_LIB rt)
    list(APPEND PLATFORM_DEFINITIONS "USE_CLOCK_GETTIME")
  else (CLOCK_GETTIME_IN_RT)
    CHECK_SYMBOL_EXISTS(GetSystemTimeAsFileTime "windows.h" WINDOWS_FILETIME)
    if (WINDOWS_FILETIME)
      list(APPEND PLATFORM_DEFINITIONS "USE_WIN_FILETIME")
    else (WINDOWS_FILETIME)
      list(APPEND PLATFORM_DEFINITIONS "USE_GETTIMEOFDAY")
    endif (WINDOWS_FILETIME)
  endif (CLOCK_GETTIME_IN_RT)
endif (CLOCK_GETTIME_IN_LIBC)

if (PN_WINAPI)
  CHECK_SYMBOL_EXISTS(strerror_s "string.h" STRERROR_S_IN_WINAPI)
  if (STRERROR_S_IN_WINAPI)
    list(APPEND PLATFORM_DEFINITIONS "USE_STRERROR_S")
  else (STRERROR_S_IN_WINAPI)
    if (MINGW)
      message (STATUS, "NOTE: your MinGW version lacks a thread safe strerror")
      list(APPEND PLATFORM_DEFINITIONS "USE_OLD_STRERROR")
    endif (MINGW)
  endif (STRERROR_S_IN_WINAPI)
else (PN_WINAPI)
  CHECK_SYMBOL_EXISTS(strerror_r "string.h" STRERROR_R_IN_LIBC)
  if (STRERROR_R_IN_LIBC)
    list(APPEND PLATFORM_DEFINITIONS "USE_STRERROR_R")
  endif (STRERROR_R_IN_LIBC)
endif (PN_WINAPI)

CHECK_SYMBOL_EXISTS(atoll "stdlib.h" C99_ATOLL)
if (C99_ATOLL)
  list(APPEND PLATFORM_DEFINITIONS "USE_ATOLL")
else (C99_ATOLL)
  CHECK_SYMBOL_EXISTS(_atoi64 "stdlib.h" WINAPI_ATOI64)
  if (WINAPI_ATOI64)
    list(APPEND PLATFORM_DEFINITIONS "USE_ATOI64")
  else (WINAPI_ATOI64)
    message(FATAL_ERROR "No atoll API found")
  endif (WINAPI_ATOI64)
endif (C99_ATOLL)

if (PN_WINAPI)
  set (PLATFORM_LIBS ws2_32 Rpcrt4)
  list(APPEND PLATFORM_DEFINITIONS "PN_WINAPI")
endif (PN_WINAPI)

# Flags for example self-test build, CACHE INTERNAL for visibility
set(C_EXAMPLE_FLAGS "${COMPILE_WARNING_FLAGS}")
set(C_EXAMPLE_LINK_FLAGS "${SANITIZE_FLAGS}")

if (CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_COMPILER_IS_GNUCC)
  # Ensure that examples build with c90, to deal with older c++03-as-c compilers.
  set(C_EXAMPLE_FLAGS "${C_EXAMPLE_FLAGS} -std=iso9899:1990 -pedantic")
endif()

if (MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(
        /wd4244
        /wd4267
        /wd4800
        /wd4996
    )
    set (qpid-proton-platform src/compiler/msvc/snprintf.c)
endif (MSVC)

# for full source distribution:
set (qpid-proton-platform-all
  src/platform/platform.c
  src/reactor/io/windows/io.c
  src/reactor/io/windows/iocp.c
  src/reactor/io/windows/write_pipeline.c
  src/reactor/io/windows/selector.c
  src/reactor/io/posix/io.c
  src/reactor/io/posix/selector.c
  )

# platform specific library build:
set (qpid-proton-platform-io
  src/platform/platform.c
  ${pn_io_impl}
  ${pn_selector_impl}
  )

# for full source distribution:
set (qpid-proton-layers-all
  src/sasl/sasl.c
  src/sasl/default_sasl.c
  src/sasl/cyrus_sasl.c
  src/sasl/cyrus_stub.c
  src/ssl/openssl.c
  src/ssl/schannel.c
  src/ssl/ssl_stub.c
  )

# for current build system's environment:
set (qpid-proton-layers
  ${pn_sasl_impl}
  ${pn_ssl_impl}
  )

set (qpid-proton-core
  src/core/object/object.c
  src/core/object/list.c
  src/core/object/map.c
  src/core/object/string.c
  src/core/object/iterator.c
  src/core/object/record.c

  src/core/log.c
  src/core/util.c
  src/core/error.c
  src/core/buffer.c
  src/core/types.c

  src/core/framing.c

  src/core/codec.c
  src/core/decoder.c
  src/core/encoder.c

  src/core/dispatcher.c
  src/core/connection_driver.c
  src/core/engine.c
  src/core/event.c
  src/core/autodetect.c
  src/core/transport.c
  src/core/message.c
  )

set (qpid-proton-include-generated
  ${CMAKE_CURRENT_BINARY_DIR}/src/encodings.h
  ${CMAKE_CURRENT_BINARY_DIR}/src/protocol.h
  ${CMAKE_CURRENT_BINARY_DIR}/include/proton/version.h
  )

set (qpid-proton-private-includes
  src/messenger/store.h
  src/messenger/subscription.h
  src/messenger/messenger.h
  src/messenger/transform.h
  src/ssl/ssl-internal.h
  src/sasl/sasl-internal.h
  src/core/autodetect.h
  src/core/log_private.h
  src/core/config.h
  src/core/encoder.h
  src/core/dispatch_actions.h
  src/core/engine-internal.h
  src/core/transport.h
  src/core/framing.h
  src/core/buffer.h
  src/core/util.h
  src/core/dispatcher.h
  src/core/data.h
  src/core/decoder.h
  src/core/max_align.h
  src/core/message-internal.h
  src/reactor/io/windows/iocp.h
  src/reactor/selector.h
  src/reactor/io.h
  src/reactor/reactor.h
  src/reactor/selectable.h
  src/platform/platform.h
  src/platform/platform_fmt.h
  src/proactor/proactor-internal.h
  )

set (qpid-proton-extra
  src/extra/url.c

  src/reactor/reactor.c
  src/reactor/handler.c
  src/reactor/connection.c
  src/reactor/acceptor.c
  src/reactor/selectable.c
  src/reactor/timer.c

  src/handlers/handshaker.c
  src/handlers/iohandler.c
  src/handlers/flowcontroller.c

  src/messenger/messenger.c
  src/messenger/subscription.c
  src/messenger/store.c
  src/messenger/transform.c
  )

set (qpid-proton-include
  include/proton/cid.h
  include/proton/codec.h
  include/proton/condition.h
  include/proton/connection.h
  include/proton/connection_driver.h
  include/proton/delivery.h
  include/proton/disposition.h
  include/proton/engine.h
  include/proton/error.h
  include/proton/event.h
  include/proton/import_export.h
  include/proton/link.h
  include/proton/listener.h
  include/proton/log.h
  include/proton/message.h
  include/proton/netaddr.h
  include/proton/object.h
  include/proton/proactor.h
  include/proton/sasl.h
  include/proton/sasl-plugin.h
  include/proton/session.h
  include/proton/ssl.h
  include/proton/terminus.h
  include/proton/transport.h
  include/proton/type_compat.h
  include/proton/types.h
  )

set (qpid-proton-include-extra
  include/proton/handlers.h
  include/proton/messenger.h
  include/proton/reactor.h
  include/proton/selectable.h
  include/proton/url.h
  )

#
# Choose a proactor: user can set PROACTOR, or if not set pick a default.
# The default is the first one that passes its build test, in order listed below.
# "none" disables the proactor even if a default is available.
#
set(PROACTOR "" CACHE STRING "Override default proactor, one of: epoll, libuv, iocp, none")
string(TOLOWER "${PROACTOR}" PROACTOR)

if (PROACTOR STREQUAL "epoll" OR (NOT PROACTOR AND NOT BUILD_PROACTOR))
  check_symbol_exists(epoll_wait "sys/epoll.h" HAVE_EPOLL)
  if (HAVE_EPOLL)
    set (PROACTOR_OK epoll)
    set (qpid-proton-proactor src/proactor/epoll.c src/proactor/proactor-internal.c)
    set (PROACTOR_LIBS Threads::Threads)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
      )
  endif()
endif()

if (PROACTOR STREQUAL "iocp" OR (NOT PROACTOR AND NOT PROACTOR_OK))
  if(WIN32 AND NOT CYGWIN)
    set (PROACTOR_OK iocp)
    set (qpid-proton-proactor src/proactor/win_iocp.c src/proactor/proactor-internal.c)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS} ${LTO}"
      COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
      )
  endif(WIN32 AND NOT CYGWIN)
endif()

if (PROACTOR STREQUAL "libuv" OR (NOT PROACTOR AND NOT PROACTOR_OK))
  find_package(Libuv)
  if (Libuv_FOUND)
    set (PROACTOR_OK libuv)
    set (qpid-proton-proactor src/proactor/libuv.c src/proactor/proactor-internal.c)
    set (PROACTOR_LIBS Libuv::Libuv)
    set_source_files_properties (${qpid-proton-proactor} PROPERTIES
      COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
      )
  endif()
endif()

if (PROACTOR_OK)
  message(STATUS "Building the ${PROACTOR_OK} proactor")
elseif (PROACTOR AND NOT PROACTOR STREQUAL "none")
  message(FATAL_ERROR "Cannot build the ${PROACTOR} proactor")
endif()

source_group("API Header Files" FILES ${qpid-proton-include} ${qpid-proton-include-extra})

set_source_files_properties (
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-extra}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS} ${LTO}"
  )

set_source_files_properties (
  ${qpid-proton-platform}
  ${qpid-proton-platform-io}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_PLATFORM_FLAGS} ${LTO}"
  COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
  )

if (BUILD_WITH_CXX)
  set_source_files_properties (
    ${qpid-proton-core}
    ${qpid-proton-proactor}
    ${qpid-proton-layers}
    ${qpid-proton-extra}
    ${qpid-proton-platform}
    ${qpid-proton-platform-io}
    PROPERTIES LANGUAGE CXX
    )
endif (BUILD_WITH_CXX)

set(qpid-proton-core-src
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-platform}
  ${qpid-proton-include}
  ${qpid-proton-include-generated}
)

add_library (qpid-proton-core SHARED ${qpid-proton-core-src})
add_dependencies (qpid-proton-core generated_c_files)
target_link_libraries (qpid-proton-core ${SSL_LIB} ${SASL_LIB} ${TIME_LIB} ${PLATFORM_LIBS})
set_target_properties (qpid-proton-core
  PROPERTIES
  VERSION   "${PN_LIB_CORE_VERSION}"
  SOVERSION "${PN_LIB_CORE_MAJOR_VERSION}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
)

if (BUILD_STATIC_LIBS)
  add_library (qpid-proton-core-static STATIC ${qpid-proton-core-src})
endif(BUILD_STATIC_LIBS)

set(qpid-proton-src
  # Proton Core
  ${qpid-proton-core}
  ${qpid-proton-layers}
  ${qpid-proton-platform}
  ${qpid-proton-include}
  ${qpid-proton-include-generated}
  # Proactor
  ${qpid-proton-proactor}
  # Proton Reactor/Messenger
  ${qpid-proton-extra}
  ${qpid-proton-platform-io}
  ${qpid-proton-include-extra}
)

add_library (qpid-proton SHARED ${qpid-proton-src})
target_link_libraries (qpid-proton LINK_PRIVATE ${SSL_LIB} ${SASL_LIB} ${TIME_LIB} ${PLATFORM_LIBS} ${PROACTOR_LIBS})
set_target_properties (qpid-proton
  PROPERTIES
  VERSION   "${PN_LIB_LEGACY_VERSION}"
  SOVERSION "${PN_LIB_LEGACY_MAJOR_VERSION}"
  LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
)

if (BUILD_STATIC_LIBS)
  add_library(qpid-proton-static STATIC ${qpid-proton-src})
endif(BUILD_STATIC_LIBS)

if (MSVC)
  # Add a phony dependency for Windows builds to serialize creation
  # of generated files. See issue PROTON-1376.
  # When a Windows build creates src/encodings.h and src/protocol.h
  # only once then this can be removed.
  add_dependencies(qpid-proton qpid-proton-core)
endif (MSVC)

if (MSVC)
  # guard against use of C99 violating functions on Windows
  include(WindowsC99CheckDef)
endif(MSVC)

if (qpid-proton-proactor)
  set(HAS_PROACTOR True)
  add_library (qpid-proton-proactor SHARED ${qpid-proton-proactor})
  target_link_libraries (qpid-proton-proactor  LINK_PUBLIC qpid-proton-core)
  target_link_libraries (qpid-proton-proactor  LINK_PRIVATE ${PLATFORM_LIBS} ${PROACTOR_LIBS})
  set_target_properties (qpid-proton-proactor
    PROPERTIES
    VERSION   "${PN_LIB_PROACTOR_VERSION}"
    SOVERSION "${PN_LIB_PROACTOR_MAJOR_VERSION}"
    LINK_FLAGS "${CATCH_UNDEFINED} ${LTO}"
  )
  if (BUILD_STATIC_LIBS)
    add_library (qpid-proton-proactor-static STATIC ${qpid-proton-proactor})
  endif(BUILD_STATIC_LIBS)
endif()

# Install executables and libraries
if (BUILD_STATIC_LIBS)
  set(STATIC_LIBS qpid-proton-static qpid-proton-core-static)
endif()
install(TARGETS qpid-proton qpid-proton-core ${STATIC_LIBS}
  EXPORT  proton
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install windows pdb files
if (MSVC)
  install(FILES
      $<TARGET_PDB_FILE:qpid-proton>
      $<TARGET_PDB_FILE:qpid-proton-core>
    DESTINATION bin
    CONFIGURATIONS RelWithDebInfo Debug
    OPTIONAL)
endif (MSVC)

if (HAS_PROACTOR)
  if (BUILD_STATIC_LIBS)
    set(STATIC_LIBS qpid-proton-proactor-static)
  endif()
  install(TARGETS qpid-proton-proactor ${STATIC_LIBS}
    EXPORT  proton
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

  # Install windows pdb files
  if (MSVC)
    install(FILES $<TARGET_PDB_FILE:qpid-proton-proactor>
      DESTINATION bin
      CONFIGURATIONS RelWithDebInfo Debug
      OPTIONAL)
  endif (MSVC)
endif ()

# Install header files
file(GLOB headers "${CMAKE_CURRENT_BINARY_DIR}/include/proton/*.[hi]")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton)

# Set ${VAR}/${VAR}DEBUG variables, configure and install the packageconf files for LIB
macro(configure_lib VAR LIB)
  if(DEFINED CMAKE_IMPORT_LIBRARY_PREFIX)
    set(LIB_PREFIX ${CMAKE_IMPORT_LIBRARY_PREFIX})
    set(LIB_SUFFIX ${CMAKE_IMPORT_LIBRARY_SUFFIX})
  else()
    set(LIB_PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX})
    set(LIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()
  set(${VAR} ${LIB_PREFIX}${LIB}${LIB_SUFFIX})
  set("${VAR}DEBUG" ${LIB_PREFIX}${LIB}${CMAKE_DEBUG_POSTFIX}${LIB_SUFFIX})
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib${LIB}.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/lib${LIB}.pc @ONLY)
  install (FILES ${CMAKE_CURRENT_BINARY_DIR}/lib${LIB}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)
endmacro()

configure_lib(PROTONLIB qpid-proton)
configure_lib(PROTONCORELIB qpid-proton-core)
if(HAS_PROACTOR)
  configure_lib(PROTONPROACTORLIB qpid-proton-proactor)
endif(HAS_PROACTOR)

include(WriteBasicConfigVersionFile)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ProtonConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfig.cmake @ONLY)
write_basic_config_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfigVersion.cmake
  VERSION ${PN_VERSION}
  COMPATIBILITY AnyNewerVersion)
install (FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ProtonConfigVersion.cmake
  DESTINATION ${LIB_INSTALL_DIR}/cmake/Proton)

add_subdirectory(docs)
add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(tools)

install (DIRECTORY examples/
         DESTINATION "${PROTON_SHARE}/examples/c"
         USE_SOURCE_PERMISSIONS
         PATTERN ProtonConfig.cmake EXCLUDE)
