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
    echo "Usage: ${ME} [DIR]"
    exit 1
}

cleanup()
{
    trap - 0 1 2 3 9 11 13 15
    echo
    [ ${WORKDIR} ] && [ -d ${WORKDIR} ] && rm -rf ${WORKDIR}
}

if [ $# == 1 ]; then
    DIR=$1
elif [ $# == 0 ]; then
    DIR=$PWD
else
    usage
fi

WORKDIR=$(mktemp -d)

##
## Create the archive
##
(
    cd ${SRC}
    TAG=$(git describe --tags --always)
    MTIME=$(date -d @`git log -1 --pretty=format:%ct tags/${TAG}` '+%Y-%m-%d %H:%M:%S')
    ARCHIVE=$DIR/qpid-proton-${TAG}.tar.gz
    [ -d ${WORKDIR} ] || mkdir -p ${WORKDIR}
    git archive --format=tar --prefix=qpid-proton-${TAG}/ tags/${TAG} \
        | tar -x -C ${WORKDIR}
    cd ${WORKDIR}
    tar -c -z \
        --owner=root --group=root --numeric-owner \
        --mtime="${MTIME}" \
        -f ${ARCHIVE} .
)
