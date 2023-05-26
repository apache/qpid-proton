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

set -e

sudo apt-get update

# OTel dependencies

sudo apt-get install libgtest-dev libgmock-dev libbenchmark-dev \
    libcurl4-openssl-dev \
    libprotobuf-dev protobuf-compiler

# Clone OpenTelemetry-cpp

git clone -b v1.9.1 --recurse-submodules https://github.com/open-telemetry/opentelemetry-cpp

# Build/Install OpenTelemetry-cpp

cd opentelemetry-cpp
mkdir build
cd build

cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_TESTING=OFF -DWITH_OTLP=ON -DWITH_OTLP_HTTP=ON -DWITH_OTLP_GRPC=OFF
cmake --build . --target all
sudo cmake --install .
cd ../..
