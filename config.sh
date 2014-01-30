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

cd $(dirname ${BASH_SOURCE[0]}) > /dev/null
export PROTON_HOME=$(pwd)
cd - > /dev/null

if [ -z "$CPROTON_BUILD" ]; then
    if [ -d $PROTON_HOME/build/proton-c ]; then
        PROTON_BINDINGS=$PROTON_HOME/build/proton-c/bindings
    else
        PROTON_BINDINGS=$PROTON_HOME/proton-c/bindings
    fi
    if [ -d $PROTON_HOME/build/proton-j ]; then
        PROTON_JARS=$PROTON_HOME/build/proton-j/proton-j.jar
    else
        PROTON_JARS=$PROTON_HOME/proton-j/proton-j.jar
    fi
else
    PROTON_BINDINGS=$CPROTON_BUILD/bindings
fi

# Python & Jython
export PYTHON_BINDINGS=$PROTON_BINDINGS/python
export COMMON_PYPATH=$PROTON_HOME/tests/python
export PYTHONPATH=$COMMON_PYPATH:$PROTON_HOME/proton-c/bindings/python:$PYTHON_BINDINGS
export JYTHONPATH=$COMMON_PYPATH:$PROTON_HOME/proton-j/src/main/resources:$PROTON_JARS
export CLASSPATH=$PROTON_JARS

# PHP
export PHP_BINDINGS=$PROTON_BINDINGS/php
if [ -d $PHP_BINDINGS ]; then
    cat <<EOF > $PHP_BINDINGS/php.ini
include_path="$PHP_BINDINGS:$PROTON_HOME/proton-c/bindings/php"
extension="$PHP_BINDINGS/cproton.so"
EOF
    export PHPRC=$PHP_BINDINGS/php.ini
fi

# Ruby
export RUBY_BINDINGS=$PROTON_BINDINGS/ruby
export RUBYLIB=$RUBY_BINDINGS:$PROTON_HOME/proton-c/bindings/ruby/lib:$PROTON_HOME/tests/ruby

# Perl
export PERL_BINDINGS=$PROTON_BINDINGS/perl
export PERL5LIB=$PERL5LIB:$PERL_BINDINGS:$PROTON_HOME/proton-c/bindings/perl/lib

# test applications
if [ -d $PROTON_HOME/build/tests/tools/apps/c ]; then
    export PATH="$PATH:$PROTON_HOME/build/tests/tools/apps/c"
fi
if [ -d $PROTON_HOME/tests/tools/apps/python ]; then
    export PATH="$PATH:$PROTON_HOME/tests/tools/apps/python"
fi

# test applications
export PATH="$PATH:$PROTON_HOME/tests/python"

# can the test harness use valgrind?
VALGRIND_EXECUTABLE="$(type -p valgrind)"
if [[ -x $VALGRIND_EXECUTABLE ]] ; then
    export VALGRIND=$VALGRIND_EXECUTABLE
fi
