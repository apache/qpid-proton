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

ME=$(basename ${0})
SRC=$(dirname $(dirname $(readlink -f $0)))

usage()
{
    echo "Usage: ${ME} [DIR]"
    exit 1
}

if [ $# == 1 ]; then
    DIR=$1
elif [ $# == 0 ]; then
    DIR=$PWD
else
    usage
fi

##
## Create the archive
##
(
    cd ${SRC}
    BRANCH=$(git symbolic-ref --short HEAD)
    ARCHIVE=$DIR/qpid-proton-${BRANCH}.tgz
    git archive --format=tgz --prefix=qpid-proton-${BRANCH}/ ${BRANCH} -o ${ARCHIVE}
)
