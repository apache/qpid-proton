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

project (Proton C CXX)

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/include")

set (qpid-proton-cpp-platform
  "src/platform"
  )

set (qpid-proton-cpp-core
    src/Connection.cpp
    src/ConnectionImpl.cpp
    src/Connector.cpp
    src/Connector.h
    src/Container.cpp
    src/ContainerImpl.cpp
    src/Endpoint.cpp
    src/Event.cpp
    src/Handler.cpp
    src/Link.cpp
    src/Acceptor.cpp
    src/Url.cpp
    src/Message.cpp
    src/MessagingAdapter.cpp
    src/MessagingEvent.cpp
    src/MessagingHandler.cpp
    src/ProtonEvent.cpp
    src/ProtonHandler.cpp
    src/Receiver.cpp
    src/Sender.cpp
    src/Session.cpp
    src/Delivery.cpp
    src/Acking.cpp
    src/Transport.cpp
    src/Logger.cpp
    src/contexts.cpp
    src/exceptions.cpp
  )

#set_source_files_properties (
#  ${qpid-proton-cpp-core}
#  PROPERTIES
#  COMPILE_FLAGS "${COMPILE_WARNING_FLAGS} ${COMPILE_LANGUAGE_FLAGS}"
#  )

set_source_files_properties (${qpid-proton-cpp-platform} PROPERTIES LANGUAGE CXX)
set_source_files_properties (
  ${qpid-proton-cpp-platform}
  PROPERTIES
  COMPILE_FLAGS "${COMPILE_PLATFORM_FLAGS}"
  COMPILE_DEFINITIONS "${PLATFORM_DEFINITIONS}"
  )



add_library (
  qpid-proton-cpp SHARED

  ${qpid-proton-cpp-core}
  ${qpid-proton-cpp-platform}

  )

target_link_libraries (qpid-proton-cpp ${PLATFORM_LIBS} qpid-proton)

set_target_properties (
  qpid-proton-cpp
  PROPERTIES
  LINKER_LANGUAGE CXX
  VERSION   "${PN_LIB_SOMAJOR}.${PN_LIB_SOMINOR}"
  SOVERSION "${PN_LIB_SOMAJOR}"
  LINK_FLAGS "${CATCH_UNDEFINED}"
  )

add_executable (HelloWorld examples/HelloWorld.cpp)
target_link_libraries (HelloWorld qpid-proton-cpp)
add_executable (HelloWorldDirect examples/HelloWorldDirect.cpp)
target_link_libraries (HelloWorldDirect qpid-proton-cpp)
add_executable (SimpleRecv examples/SimpleRecv.cpp)
target_link_libraries (SimpleRecv qpid-proton-cpp)
add_executable (SimpleSend examples/SimpleSend.cpp)
target_link_libraries (SimpleSend qpid-proton-cpp)

install (TARGETS qpid-proton-cpp
  EXPORT  proton
  ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
  LIBRARY DESTINATION ${LIB_INSTALL_DIR})

# Install windows qpid-proton-cpp pdb files
if (MSVC)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/qpid-proton${CMAKE_DEBUG_POSTFIX}.pdb
    DESTINATION bin
    CONFIGURATIONS Debug
    OPTIONAL)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/qpid-proton.pdb
    DESTINATION bin
    CONFIGURATIONS RelWithDebInfo
    OPTIONAL)
endif (MSVC)

# Install header files
file(GLOB headers "include/proton/cpp/*.h")
install (FILES ${headers} DESTINATION ${INCLUDE_INSTALL_DIR}/proton/cpp)
