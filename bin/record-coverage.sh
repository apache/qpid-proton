#! /usr/bin/env bash

# This script collates coverage data already present from running instrumented code.
#
# It requires the lcov tool to be installed (this provides the lcov and genhtml commands)
#
# It will produce a coverage analysis for gcc or clang compiled builds and currently for
# C and C++ parts of the build tree.
#
# It takes two command line arguments:
# - The first is the proton source tree: this is mandatory.
# - The second is the build tree: this is optional and if not specified is assumed to be the
#   current directory.
#
# The output is in the form of an html report which will be found in the generated html directory.
# - There will also be a number of intermediate files left in the current directory.
#
# The typical way to use it would be to use the "Coverage" build type to get instrumented
# code, then to run the tests then to extract the coverage information from running the
# tests.
# Something like:
#   cmake $PROTON_SRC -DCMAKE_BUILD_TYPE=Coverage
#   make
#   make test
#   make coverage

# set -x

# get full path
function getpath {
  pushd -n $1 > /dev/null
  echo $(dirs -0 -l)
  popd -n > /dev/null
}

SRC=${1?}
BLD=${2:-.}

BLDPATH=$(getpath $BLD)
SRCPATH=$(getpath $SRC)

# Get base profile
# - this initialises 0 counts for every profiled file
#   without this step any file with no counts at all wouldn't
#   show up on the final output.
lcov -c -i -d $BLDPATH -o proton-base.info

# Get actual coverage data
lcov -c -d $BLDPATH -o proton-ctest.info

# Total them up
lcov --add proton-base.info --add proton-ctest.info > proton-total-raw.info

# Snip out stuff in /usr (we don't care about coverage in system code)
lcov --remove proton-total-raw.info "/usr/include*" "/usr/share*" > proton-total.info

# Generate report
rm -rf html
genhtml -p $SRCPATH -p $BLDPATH proton-total.info --title "Proton CTest Coverage" --demangle-cpp -o html

