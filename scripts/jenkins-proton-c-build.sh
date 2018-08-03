#!/bin/bash -e
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

# This is the continuous delivery build script executed after a git
# extract by the Jenkins build process located at the following URL:
# https://builds.apache.org/view/M-R/view/Qpid/job/Qpid-proton-c/
#
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_INSTALL_PREFIX=$PWD/build/ship"
XMLOUTPUT=../testresults/TEST-protonc.xml

echo Arch: `arch` Uname: `uname -a` lsb_release: `lsb_release -a` User: `whoami`

echo =========================
echo Listing installed packages
dpkg -l | \
  awk '/^ii  (cmake |maven |ruby |python |.*jdk |swig[0-9]*)/{print $2, $3}'| \
  sort
echo =========================

which python || exit 1
which swig || exit 1

# if python-pip is available, install the python tox test tool
RUN_TOX=false
PIP=$(type -p pip || true)
if [ -n $PIP ] && [ -x "$PIP" ]; then
    ldir=$(python -c 'import site; print("%s" % site.USER_BASE)')
    PATH="$ldir/bin:$PATH"
    echo "PATH=$PATH"
    if [ $VIRTUAL_ENV ]; then
      pip install -U pip
      pip install -U tox
    else
      pip install --user -U pip
      pip install --user -U tox
    fi
    RUN_TOX=true
fi

ls

rm -rf build testresults >/dev/null 2>&1
mkdir build testresults >/dev/null 2>&1

cd build >/dev/null 2>&1

cmake ${CMAKE_FLAGS} ..
cmake --build . --target install

echo Running tests

$RUN_TOX && ctest -V -R 'python-tox-test'

source config.sh

# proton-c tests via python
python ../python/tests/proton-test --xml=${XMLOUTPUT}

# proton-c native c-* tests
ctest -V -R '^c-*'

echo 'Build completed'
