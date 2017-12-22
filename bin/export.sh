#!/bin/bash

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

# export.sh - Create a release archive.
set -e
trap "cleanup" 0 1 2 3 9 11 13 15

ME=$(basename ${0})
SRC=$(dirname $(dirname $(readlink -f $0)))

usage()
{
    echo
    echo "Usage: ${ME} [DIR] [TAG]"
    exit 1
}

cleanup()
{
    trap - 0 1 2 3 9 11 13 15
    echo
    [ ${WORKDIR} ] && [ -d ${WORKDIR} ] && rm -rf ${WORKDIR}
}

DIR=$PWD
TAG=$(git describe --tags --always)

##
## Allow overrides to be passed on the cmdline
##
if [ $# -gt 2 ]; then
    usage
elif [ $# -ge 1 ]; then
    DIR=$1
    if [ $# -eq 2 ]; then
        TAG=$2
    fi
fi

# verify the tag exists
git rev-list -1 tags/${TAG} -- >/dev/null || usage

WORKDIR=$(mktemp -d)

##
## Create the archive
##
(
    cd ${SRC}
    MTIME=$(date -d @`git log -1 --pretty=format:%ct tags/${TAG}` '+%Y-%m-%d %H:%M:%S')
    VERSION=$(git show tags/${TAG}:VERSION.txt)
    ARCHIVE=$DIR/qpid-proton-${VERSION}.tar.gz
    PREFIX=qpid-proton-${VERSION}
    [ -d ${WORKDIR} ] || mkdir -p ${WORKDIR}
    git archive --format=tar --prefix=${PREFIX}/ tags/${TAG} \
        | tar -x -C ${WORKDIR}
    cd ${WORKDIR}
    tar -c -z \
        --owner=root --group=root --numeric-owner \
        --mtime="${MTIME}" \
        -f ${ARCHIVE} ${PREFIX}
    echo "${ARCHIVE}"
)
