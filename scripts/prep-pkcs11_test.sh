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

# prep-pkcs11_test.sh - Source to set up environment for pkcs11_test to run
#                       against a SoftHSM

set -x

KEYDIR="$(readlink -f cpp/testdata/certs)"

if [ -z "$PKCS11_PROVIDER" ]; then
    export PKCS11_PROVIDER=$(openssl version -m | cut -d'"' -f2)/pkcs11.so
fi

if [ -z "$PKCS11_PROVIDER_MODULE" ]; then
    export PKCS11_PROVIDER_MODULE="/usr/lib/softhsm/libsofthsm2.so"
fi

PKCS11_PROVIDER=$(readlink -f "$PKCS11_PROVIDER")
PKCS11_PROVIDER_MODULE=$(readlink -f "$PKCS11_PROVIDER_MODULE")

if [ ! -r "$PKCS11_PROVIDER" ]; then
    echo "PKCS11_PROVIDER=$PKCS11_PROVIDER not found"
    return 1
fi

if [ ! -r "$PKCS11_PROVIDER_MODULE" ]; then
    echo "PKCS11_PROVIDER_MODULE=$PKCS11_PROVIDER_MODULE not found"
    return 1
fi

export OPENSSL_CONF="$(readlink -f scripts/openssl-pkcs11.cnf)"
export SOFTHSM2_CONF="${XDG_RUNTIME_DIR}/qpid-proton-build/softhsm2.conf"

softhsmtokendir="${XDG_RUNTIME_DIR}/qpid-proton-build/softhsm2-tokens"
mkdir -p "${softhsmtokendir}"

sed -r "s;@softhsmtokendir@;${softhsmtokendir};g" scripts/softhsm2.conf.in >$SOFTHSM2_CONF

export PKCS11_MODULE_LOAD_BEHAVIOR=late

set -x

softhsm2-util --delete-token --token proton-test 2>/dev/null || true
softhsm2-util --init-token --free --label proton-test --pin tclientpw --so-pin tclientpw

pkcs11_tool () { pkcs11-tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw "$@"; }

pkcs11_tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw -l --label tclient --delete-object --type privkey 2>/dev/null || true

pkcs11_tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw -l --label tclient --id 2222 \
    --write-object "$KEYDIR/client-certificate.pem" --type cert --usage-sign
pkcs11_tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw -l --label tclient --id 2222 \
    --write-object "$KEYDIR/client-private-key-no-password.pem" --type privkey --usage-sign

pkcs11_tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw -l --label tserver --id 4444 \
    --write-object "$KEYDIR/server-certificate-lh.pem" --type cert --usage-sign
pkcs11_tool --module=$PKCS11_PROVIDER_MODULE --token-label proton-test --pin tclientpw -l --label tserver --id 4444 \
    --write-object "$KEYDIR/server-private-key-lh-no-password.pem" --type privkey --usage-sign

set +x

# Workaround for https://github.com/latchset/pkcs11-provider/issues/419
export PKCS11_MODULE_LOAD_BEHAVIOR=early

export PKCS11_CLIENT_CERT="pkcs11:token=proton-test;object=tclient;type=cert"
export PKCS11_CLIENT_KEY="pkcs11:token=proton-test;object=tclient;type=private"
export PKCS11_SERVER_CERT="pkcs11:token=proton-test;object=tserver;type=cert"
export PKCS11_SERVER_KEY="pkcs11:token=proton-test;object=tserver;type=private"
export PKCS11_CA_CERT="$KEYDIR/ca-certificate.pem"
