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

if test $# -lt 2; then
    echo <<EOF
usage: $0 LIB EXE [args ...]
Get the libasan linked to LIB and preload it to run `EXE args ...`
EOF
fi

LIB=$1; shift
EXE=$2

case $EXE in
    *ruby*|*.rb|*python*|*.py)
        # ruby has spurious leaks and causes asan errors.
        #
        # python tests have many leaks that may be real, but need to be
        # analysed & fixed or suppressed before turning this on

        # Disable link order check to run with limited sanitizing
        # Still seeing problems.
        export ASAN_OPTIONS=verify_asan_link_order=0
        ;;
    *)
        # Preload the asan library linked to LIB. Note we need to
        # check the actual linkage, there may be multiple asan lib
        # versions installed and we must use the same one.
        libasan=$(ldd $LIB | awk "/(libasan\\.so[.0-9]*)/ { print \$1 }")
        export LD_PRELOAD="$libasan:$LD_PRELOAD"
        ;;
esac

exec "$@"
