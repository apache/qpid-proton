# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

FROM ossfuzz/base-builder
MAINTAINER jdanek@redhat.com
RUN apt-get install -y \
    cmake \
    libuv1-dev
    # (optional) add vim

# (optional) customize enviromental variables
#ENV FUZZING_ENGINE
#ENV SANITIZER_FLAGS

# copy qpid-proton from filesystem into the container
COPY . ./qpid-proton
WORKDIR /src/qpid-proton

# refresh the build directory if it exists already
RUN rm build -rf || true

# /usr/local/bin/compile compiles libFuzzer, then calls /src/build.sh
# and sets correct environment variables for it
RUN echo cmake .. -DCMAKE_BUILD_TYPE=Debug -DFUZZ_TEST=ON -DFUZZING_ENGINE=ON > /src/build.sh

# build it
RUN mkdir build
WORKDIR /src/qpid-proton/build
RUN /usr/local/bin/compile
WORKDIR /src/qpid-proton/build/proton-c/src/tests/fuzz
RUN make
RUN ls

# run corpus through fuzzer and irrespective of result start bash
ENTRYPOINT make test; bash
