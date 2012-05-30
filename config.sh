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

export PROTON_HOME=$(dirname $(readlink -f $0))

if [ -z "$CPROTON_BUILD" ]; then
    if [ -d $PROTON_HOME/proton-c/build ]; then
        export PYTHON_BINDINGS=$PROTON_HOME/proton-c/build/bindings/python
    else
        export PYTHON_BINDINGS=$PROTON_HOME/proton-c/bindings/python
    fi
else
    export PYTHON_BINDINGS=$CPROTON_BUILD/bindings/python
fi

export PYTHONPATH=$PROTON_HOME/tests:$PROTON_HOME/proton-c:$PYTHON_BINDINGS
export JYTHONPATH=$PROTON_HOME/tests:$PROTON_HOME/proton-j:$PROTON_HOME/proton-j/dist/lib/qpidproton.jar
