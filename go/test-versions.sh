#!/bin/bash
#
# Takes a list of install prefixes and tests the go source under the current directory
# against the proton install in each prefix.
#
# NOTE: This script will fail if it finds proton headers or libraries installed in standard
# places or on the existing paths, to avoid possible confusion.
#

for VAR in LD_LIBRARY_PATH LIBRARY_PATH C_INCLUDE_PATH; do
    declare OLD_${VAR}=${!VAR};
done

prefix() {
    prefix=$1
    export LD_LIBRARY_PATH="$prefix/lib64:$prefix/lib:$OLD_LD_LIBRARY_PATH"
    export LIBRARY_PATH="$prefix/lib64:$prefix/lib:$OLD_LIBRARY_PATH"
    export C_INCLUDE_PATH="$prefix/include:$OLD_C_INCLUDE_PATH"
}

TEMP=$(mktemp -d)
trap "rm -rf $TEMP"  EXIT
set -o pipefail

cat > $TEMP/test.c  <<EOF
#include <proton/connection.h>
int main(int c, char **a) { return 0; }
EOF
cc $TEMP/test.c 2> /dev/null && {
    echo "cc found proton in include path"; cc -E | grep proton/connection.h | head -n1; exit 1; } 1>&2

cat > $TEMP/test.c  <<EOF
int main(int c, char **a) { return 0; }
EOF
cc -lqpid-proton $TEMPC 2>/dev/null && { echo "cc found proton in library path" 1>&2 ; exit 1; }

for P in "$@"; do
    (
        case $P in
            /*) ;;
            *) P=$PWD/$P;;
        esac
        test -d $P || { echo "no such directory: $P"; continue; }
        echo ==== $P
        prefix $P
        export GOPATH=$PWD
        git clean -dfx
        go test qpid.apache.org/...
    )
done
