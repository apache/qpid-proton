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

# release.sh - Creates a release.

ME=$(basename ${0})
CURRDIR=$PWD
SRC=$(dirname $(dirname $(readlink -f $0)))

usage()
{
    echo "Usage: ${ME} VERSION TAG"
    exit 1
}

if [ $# == 2 ]; then
    VERSION=$1
    TAG=$2
else
    usage
fi

die()
{
    printf "ERROR: %s\n" "$*"
    exit 1
}

##
## Create the tag
##
(
    cd ${SRC}
    if [ -n "$(git status -uno --porcelain)" ]; then
        die must release from a clean checkout
    fi
    BRANCH=$(git symbolic-ref -q --short HEAD)
    if [ -n "${BRANCH}" ]; then
        REMOTE=$(git config branch.${BRANCH}.remote)
    else
        REMOTE="origin"
    fi
    git checkout --detach && \
        bin/version.sh $VERSION && \
        git commit -a -m "Release $VERSION" && \
        git tag -m "Release $VERSION" $TAG && \
        echo "Run 'git push ${REMOTE} ${TAG}' to push the tag upstream."
)
