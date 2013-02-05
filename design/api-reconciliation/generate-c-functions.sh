#!/bin/sh

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

# Script to generate a list of proton-c functions for use as input to the api-reconciliation tool.

# If you have problems running ctags, note that there are two ctags executables on some Linux
# distributions. The one required here is from the exuberant-ctags package
# (http://ctags.sourceforge.net), *not* GNU emacs ctags.

BASE_DIR=`dirname $0`
INCLUDE_DIR=$BASE_DIR/../../proton-c/include/proton
OUTPUT_DIR=$BASE_DIR/target

mkdir -p $OUTPUT_DIR
ctags --c-kinds=p -x $INCLUDE_DIR/*.h | awk '{print $1'} > $OUTPUT_DIR/cfunctions.txt
