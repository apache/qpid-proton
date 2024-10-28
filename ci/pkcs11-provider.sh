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

# pkcs11-provider dependencies

sudo apt-get install meson

# Clone pkcs11-provider

git clone -b v0.5 https://github.com/latchset/pkcs11-provider

# Build/Install pkcs11-provider

cd pkcs11-provider
mkdir build

meson setup build .
meson compile -C build
meson install -C build
cd ..
