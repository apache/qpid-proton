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

import os

import cffi.pkgconfig

from distutils import ccompiler
from cffi import FFI


ffibuilder = FFI()

# cdef() expects a single string declaring the C types, functions and
# globals needed to use the shared object. It must be in valid C syntax
# with cffi extensions
cdefs = open('cproton.h').read()
ffibuilder.cdef(cdefs)

proton_base = '.'
proton_c_src = os.path.join(proton_base, 'src')
proton_core_src = os.path.join(proton_c_src, 'core')
proton_c_include = os.path.join(proton_base, 'include')

sources = []
extra = []
libraries = []
for root, _, files in os.walk(proton_core_src):
    for file_ in files:
        if file_.endswith(('.c', '.cpp')):
            sources.append(os.path.join(root, file_))

compiler_type = ccompiler.get_default_compiler()

if compiler_type == 'msvc':
    sources += [
        os.path.join(proton_c_src, 'compiler', 'msvc', 'start.c')
    ]
elif compiler_type == 'unix':
    sources += [
        os.path.join(proton_c_src, 'compiler', 'gcc', 'start.c')
    ]
    extra += ['-std=c99']

sources.append(os.path.join(proton_c_src, 'sasl', 'sasl.c'))
sources.append(os.path.join(proton_c_src, 'sasl', 'default_sasl.c'))

pkgconfig = []
if os.name == 'nt':
    libraries += ['crypt32', 'secur32']
    sources.append(os.path.join(proton_c_src, 'ssl', 'schannel.cpp'))
else:
    try:
        ssl_pkgcfg = cffi.pkgconfig.flags_from_pkgconfig(['openssl'])
        sources.append(os.path.join(proton_c_src, 'ssl', 'openssl.c'))
        pkgconfig.append('openssl')
    except cffi.pkgconfig.PkgConfigError:
        # Stub ssl
        sources.append(os.path.join(proton_c_src, 'ssl', 'ssl_stub.c'))

# Stub sasl
try:
    sasl_pkgcfg = cffi.pkgconfig.flags_from_pkgconfig(['libsasl2'])
    sources.append(os.path.join(proton_c_src, 'sasl', 'cyrus_sasl.c'))
    pkgconfig.append('libsasl2')
except cffi.pkgconfig.PkgConfigError:
    sources.append(os.path.join(proton_c_src, 'sasl', 'cyrus_stub.c'))

include_dirs = [proton_c_include, proton_c_src]
macros = [('PROTON_DECLARE_STATIC', None)]

c_code = r"""
#include "proton/version.h"
#include "proton/types.h"
#include "proton/object.h"
#include "proton/error.h"
#include "proton/condition.h"
#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"
#include "proton/terminus.h"
#include "proton/delivery.h"
#include "proton/disposition.h"
#include "proton/transport.h"
#include "proton/event.h"
#include "proton/message.h"
#include "proton/sasl.h"
#include "proton/ssl.h"
#include "proton/codec.h"
#include "proton/connection_driver.h"
#include "proton/cid.h"

static void pn_pyref_incref(void *object);
static void pn_pyref_decref(void *object);

static int pn_pyref_refcount(void *object) {
    return 1;
}

pn_connection_t *pn_cast_pn_connection(void *x) { return (pn_connection_t *) x; }
pn_session_t *pn_cast_pn_session(void *x) { return (pn_session_t *) x; }
pn_link_t *pn_cast_pn_link(void *x) { return (pn_link_t *) x; }
pn_delivery_t *pn_cast_pn_delivery(void *x) { return (pn_delivery_t *) x; }
pn_transport_t *pn_cast_pn_transport(void *x) { return (pn_transport_t *) x; }

static pn_class_t* PN_PYREF;
PN_HANDLE(PN_PYCTX);

static pn_class_t* pn_create_pyref() {
    return pn_class_create("pn_pyref", NULL, NULL, pn_pyref_incref, pn_pyref_decref, pn_pyref_refcount);
}

pn_event_t *pn_collector_put_py(pn_collector_t *collector, void *context, pn_event_type_t type) {
    return pn_collector_put(collector, PN_PYREF, context, type);
}

void pn_record_def_py(pn_record_t *record) {
    pn_record_def(record, PN_PYCTX, PN_PYREF);
}

void *pn_record_get_py(pn_record_t *record) {
    return pn_record_get(record, PN_PYCTX);
}

void pn_record_set_py(pn_record_t *record, void *value) {
    pn_record_set(record, PN_PYCTX, value);
}

ssize_t pn_message_encode_py(pn_message_t *msg, char *bytes, size_t size) {
    int err = pn_message_encode(msg, bytes, &size);
    if (err == 0) return size;
    else return err;
}

ssize_t pn_data_format_py(pn_data_t *data, char *bytes, size_t size) {
    int err = pn_data_format(data, bytes, &size);
    if (err == 0) return size;
    else return err;
}

int pn_ssl_get_peer_hostname_py(pn_ssl_t *ssl, char *hostname, size_t size) {
    return pn_ssl_get_peer_hostname(ssl, hostname, &size);
}

const char *pn_event_class_name_py(pn_event_t *event) {
    const pn_class_t *class = pn_event_class(event);
    return class ? pn_class_name(class) : 0;
}

void init() {
    PN_PYREF = pn_create_pyref();
}


"""

if len(pkgconfig) == 0:
    ffibuilder.set_source(
        "cproton_ffi",
        c_code,
        define_macros=macros,
        extra_compile_args=extra,
        sources=sources,
        include_dirs=include_dirs,
        libraries=libraries
    )
else:
    ffibuilder.set_source_pkgconfig(
        "cproton_ffi",
        pkgconfig,
        c_code,
        define_macros=macros,
        extra_compile_args=extra,
        sources=sources,
        include_dirs=include_dirs
    )

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
