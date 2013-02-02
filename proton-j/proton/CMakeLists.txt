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

set(BOUNCYCASTLE_VERSION 1.47 CACHE STRING "Version of Bouncycastle to compile proton-j-impl against")

find_jar(BOUNCYCASTLE_BCPKIX_JAR
         NAMES bcpkix-jdk15on bcpkix
         PATHS ${PROTON_JAR_DEPEND_DIR}
         VERSIONS ${BOUNCYCASTLE_VERSION}
         DOC "Full path to Bouncycastle PKIX jar")
find_jar(BOUNCYCASTLE_BCPROV_JAR
         NAMES bcprov-jdk15on bcprov
         PATHS ${PROTON_JAR_DEPEND_DIR}
         VERSIONS ${BOUNCYCASTLE_VERSION}
         DOC "Full path to Bouncycastle Provider jar")

if (${BOUNCYCASTLE_BCPKIX_JAR} MATCHES "^.*-NOTFOUND$" OR ${BOUNCYCASTLE_BCPROV_JAR} MATCHES "^.*-NOTFOUND$")
  set(BOUNCYCASTLE_FOUND FALSE)
else()
  set(BOUNCYCASTLE_FOUND TRUE)
endif()

message("Locations of Bouncycastle ${BOUNCYCASTLE_VERSION} jars: "
        "${BOUNCYCASTLE_BCPROV_JAR} ${BOUNCYCASTLE_BCPKIX_JAR}")

if (BOUNCYCASTLE_FOUND)
  message("Will build proton-j-impl")
  set(CMAKE_JAVA_TARGET_VERSION ${PN_VERSION})

  file(GLOB_RECURSE SOURCES_ABS "src/main/java/*.java")

  set(CMAKE_JAVA_INCLUDE_PATH
        ${PROTON_API_TARGET_JAR}
        ${BOUNCYCASTLE_BCPROV_JAR}
        ${BOUNCYCASTLE_BCPKIX_JAR})

  add_jar(proton-j-impl ${SOURCES_ABS})

  rebuild_jar(proton-j-impl proton-j-impl-${PN_VERSION}.jar)

  add_dependencies(proton-j-impl proton-api)
else()
    message("Won't build proton-j-impl because one or more Bouncycastle jars were not found. PROTON_JAR_DEPEND_DIR was: ${PROTON_JAR_DEPEND_DIR}")
endif()
