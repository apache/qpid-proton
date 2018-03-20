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

# version.sh - Sets the version of the proton source tree to the given
#              value.

ME=$(basename ${0})
usage()
{
    echo "Usage: ${ME} [SRC] VERSION"
    exit 1
}

if [ $# == 2 ]; then
    SRC=$1
    VERSION=$2
elif [ $# == 1 ]; then
    SRC=$(dirname $(dirname $(readlink -f $0)))
    VERSION=$1
else
    usage
fi

echo ${VERSION} > ${SRC}/VERSION.txt
