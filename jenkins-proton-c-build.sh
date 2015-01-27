#!/bin/bash -e
#
# This is the continuous delivery build script executed after a git
# extract by the Jenkins build process located at the following URL:
# https://builds.apache.org/view/M-R/view/Qpid/job/Qpid-proton-c/
#

echo Arch: `arch` Uname: `uname -a` lsb_release: `lsb_release -a` User: `whoami`
echo Java home: $JAVA_HOME

echo =========================
echo Listing installed packages
dpkg -l | \
  awk '/^ii  (cmake |ruby |python |php |.*jdk |swig[0-9]*)/{print $2, $3}'| \
  sort
echo =========================

which python || exit 1
which swig || exit 1

set -x
ls

rm -rf build testresults >/dev/null 2>&1
mkdir build testresults >/dev/null 2>&1
cd build && cmake ..
make all

echo Running tests
XMLOUTPUT=testresults/TEST-protonc.xml

. build/config.sh
./tests/python/proton-test --xml=${XMLOUTPUT}

cd build
ctest -V -R "^c-*"

echo 'Build completed'
