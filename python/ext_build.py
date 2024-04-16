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

from cffi import FFI


ffibuilder = FFI()

# cdef() expects a single string declaring the C types, functions and
# globals needed to use the shared object. It must be in valid C syntax
# with cffi extensions
cdefs = open('cproton.h').read()
ffibuilder.cdef(cdefs)

sources = []
extra = []
libraries = []
pkgconfig = []

proton_base = '.'
proton_c_src = os.path.join(proton_base, 'src')
proton_core_src = os.path.join(proton_c_src, 'core')
proton_c_include = os.path.join(proton_base, 'include')

for root, dirs, files in os.walk(proton_core_src):
    dirs.sort()  # needed for os.walk to process directories in deterministic order
    files.sort()
    for file_ in files:
        if file_.endswith(('.c', '.cpp')):
            sources.append(os.path.join(root, file_))

if os.name == 'nt':
    sources += [
        os.path.join(proton_c_src, 'compiler', 'msvc', 'start.c')
    ]
elif os.name == 'posix':
    sources += [
        os.path.join(proton_c_src, 'compiler', 'gcc', 'start.c')
    ]
    extra += ['-std=c99']

sources.append(os.path.join(proton_c_src, 'sasl', 'sasl.c'))
sources.append(os.path.join(proton_c_src, 'sasl', 'default_sasl.c'))

if os.name == 'nt':
    libraries += ['crypt32', 'secur32']
    sources.append(os.path.join(proton_c_src, 'ssl', 'schannel.cpp'))
else:
    try:
        # This is just used to test if pkgconfig finds openssl, if not it will throw
        cffi.pkgconfig.flags_from_pkgconfig(['openssl'])
        sources.append(os.path.join(proton_c_src, 'ssl', 'openssl.c'))
        pkgconfig.append('openssl')
    except cffi.pkgconfig.PkgConfigError:
        # Stub ssl
        sources.append(os.path.join(proton_c_src, 'ssl', 'ssl_stub.c'))

# Stub sasl
try:
    # This is just used to test if pkgconfig finds cyrus sasl, if not it will throw
    cffi.pkgconfig.flags_from_pkgconfig(['libsasl2'])
    sources.append(os.path.join(proton_c_src, 'sasl', 'cyrus_sasl.c'))
    pkgconfig.append('libsasl2')
except cffi.pkgconfig.PkgConfigError:
    sources.append(os.path.join(proton_c_src, 'sasl', 'cyrus_stub.c'))

include_dirs = [proton_c_include, proton_c_src]
macros = [('PROTON_DECLARE_STATIC', None)]

c_code = open('cproton_ext.c').read()

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
