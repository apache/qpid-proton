#!/bin/bash -e
#
# This is the continuous delivery build script executed after a git
# extract by the Jenkins build process located at the following URL:
# https://builds.apache.org/view/M-R/view/Qpid/job/Qpid-proton-c/
#
CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_INSTALL_PREFIX=$PWD/build/ship"
XMLOUTPUT=../testresults/TEST-protonc.xml

echo Arch: `arch` Uname: `uname -a` lsb_release: `lsb_release -a` User: `whoami`
echo Java home: $JAVA_HOME

echo =========================
echo Listing installed packages
dpkg -l | \
  awk '/^ii  (cmake |maven |ruby |python |php |.*jdk |swig[0-9]*)/{print $2, $3}'| \
  sort
echo =========================

which python || exit 1
which swig || exit 1

# if python-pip is available, install the python tox test tool
RUN_TOX=false
PIP=$(type -p pip)
if [[ -n "$PIP" ]] && [[ -x "$PIP" ]]; then
    ldir=$(python -c 'import site; print("%s" % site.USER_BASE)')
    PATH="$ldir/bin:$PATH"
    echo "PATH=$PATH"
    pip install --user -U tox
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
python ../tests/python/proton-test --xml=${XMLOUTPUT}

# proton-c native c-* tests
ctest -V -R '^c-*'

# proton-j tests via jython
which mvn && ctest -V -R proton-java

echo 'Build completed'
