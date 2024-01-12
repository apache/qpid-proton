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

from cffi import FFI

bld_tree_top = os.environ.get("CMAKE_BINARY_DIR")
bld_clibdir = os.environ.get("QPID_PROTON_CORE_TARGET_DIR")
cdefs = open('cproton.h').read()
c_code = open('cproton_ext.c').read()
extra_link_args = [f"-Wl,-rpath,{bld_clibdir}"] if os.name == 'posix' else None
ffibuilder = FFI()
ffibuilder.cdef(cdefs)
ffibuilder.set_source(
    "cproton_ffi",
    c_code,
    include_dirs=[f"{bld_tree_top}/c/include"],
    library_dirs=[f"{bld_clibdir}"],
    libraries=["qpid-proton-core"],
    extra_link_args=extra_link_args,
)

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
