#!/usr/bin/env python
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

"""
python-qpid-proton setup script

DISCLAIMER: This script took lots of inspirations from PyZMQ, which is licensed
under the 'MODIFIED BSD LICENSE'.

Although inspired by the work in PyZMQ, this script and the modules it depends
on were largely simplified to meet the requirements of the library.

The behavior of this script is to build the registered `_cproton` extension
using the installed Qpid Proton C library and header files. If the library and
headers are not installed, or the installed version does not match the version
of these python bindings, then the script will attempt to build the extension
using the Proton C sources included in the python source distribution package.
"""

import os

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

from setuputils import log
from setuputils import misc


class BuildExtension(build_ext):
    description = "Build Qpid Proton extension"

    def use_bundled_proton(self):
        """The proper version of libqpid-proton-core is not installed on the system,
        so use the included proton-c sources to build the extension
        """
        log.info("Building the bundled proton-c sources into the extension")

        setup_path = os.path.dirname(os.path.realpath(__file__))
        base = self.get_finalized_command('build').build_base
        build_include = os.path.join(base, 'include')
        proton_base = os.path.relpath(setup_path)
        proton_src = os.path.join(proton_base, 'src')
        proton_core_src = os.path.join(proton_base, 'src', 'core')
        proton_include = os.path.join(proton_base, 'include')

        log.debug("Using Proton C sources: %s" % proton_base)

        # Collect all the Proton C files packaged in the sdist and strip out
        # anything windows and configuration-dependent

        sources = []
        for root, _, files in os.walk(proton_core_src):
            for file_ in files:
                if file_.endswith(('.c', '.cpp')):
                    sources.append(os.path.join(root, file_))

        # Look for any optional libraries that proton needs, and adjust the
        # source list and compile flags as necessary.
        library_dirs = []
        libraries = []
        includes = []
        macros = []
        extra = []

        # -D flags (None means no value, just define)
        macros += [('PROTON_DECLARE_STATIC', None)]

        cc = self.compiler

        if cc.compiler_type == 'msvc':
            sources += [
                os.path.join(proton_src, 'compiler', 'msvc', 'start.c')
            ]
        elif cc.compiler_type == 'unix':
            sources += [
                os.path.join(proton_src, 'compiler', 'gcc', 'start.c')
            ]
            extra += ['-std=c99']

        # Check whether openssl is installed by poking
        # pkg-config for a minimum version 0. If it's installed, it should
        # return True and we'll use it. Otherwise, we'll use the stub.
        if misc.pkg_config_version_installed('openssl', atleast='0'):
            library_dirs += [misc.pkg_config_get_var('openssl', 'libdir')]
            libraries += ['ssl', 'crypto']
            includes += [misc.pkg_config_get_var('openssl', 'includedir')]
            sources.append(os.path.join(proton_src, 'ssl', 'openssl.c'))
        elif os.name == 'nt':
            libraries += ['crypt32', 'secur32']
            sources.append(os.path.join(proton_src, 'ssl', 'schannel.cpp'))
        else:
            sources.append(os.path.join(proton_src, 'ssl', 'ssl_stub.c'))
            log.warn("OpenSSL not installed - disabling SSL support!")

        sources.append(os.path.join(proton_src, 'sasl', 'sasl.c'))
        sources.append(os.path.join(proton_src, 'sasl', 'default_sasl.c'))

        # Check whether cyrus sasl is installed by asking pkg-config
        # This works for all recent versions of cyrus sasl
        if misc.pkg_config_version_installed('libsasl2', atleast='0'):
            library_dirs += [misc.pkg_config_get_var('libsasl2', 'libdir')]
            libraries.append('sasl2')
            includes += [misc.pkg_config_get_var('libsasl2', 'includedir')]
            sources.append(os.path.join(proton_src, 'sasl', 'cyrus_sasl.c'))
        else:
            sources.append(os.path.join(proton_src, 'sasl', 'cyrus_stub.c'))
            log.warn("Cyrus SASL not installed - only the ANONYMOUS and PLAIN mechanisms will be supported!")

        # compile all the proton sources.  We'll add the resulting list of
        # objects to the _cproton extension as 'extra objects'.  We do this
        # instead of just lumping all the sources into the extension to prevent
        # any proton-specific compilation flags from affecting the compilation
        # of the generated swig code
        objects = cc.compile(sources,
                             macros=macros,
                             include_dirs=[build_include,
                                           proton_include,
                                           proton_src] + includes,
                             # compiler command line options:
                             extra_preargs=extra,
                             output_dir=self.build_temp)

        #
        # Now update the _cproton extension instance passed to setup to include
        # the objects and libraries
        #
        _cproton = self.distribution.ext_modules[-1]
        _cproton.extra_objects = objects
        _cproton.include_dirs.append(build_include)
        _cproton.include_dirs.append(proton_include)

        # lastly replace the libqpid-proton-core dependency with libraries required
        # by the Proton objects:
        _cproton.library_dirs = library_dirs
        _cproton.libraries = libraries
        _cproton.extra_compile_args = ['-DPROTON_DECLARE_STATIC']

    def libqpid_proton_installed(self):
        """Check to see if the proper version of the Proton development library
        and headers are already installed
        """
        return misc.pkg_config_version_installed('libqpid-proton-core', atleast='0.38')

    def use_installed_proton(self):
        """The Proton development headers and library are installed, update the
        _cproton extension to tell it where to find the library and headers.
        """
        # update the Extension instance passed to setup() to use the installed
        # headers and link library
        _cproton = self.distribution.ext_modules[-1]
        incs = misc.pkg_config_get_var('libqpid-proton-core', 'includedir')
        for i in incs.split():
            _cproton.swig_opts.append('-I%s' % i)
            _cproton.include_dirs.append(i)
        ldirs = misc.pkg_config_get_var('libqpid-proton-core', 'libdir')
        _cproton.library_dirs.extend(ldirs.split())

    def build_extensions(self):
        # check if the Proton library and headers are installed and are
        # compatible with this version of the binding.
        if self.libqpid_proton_installed():
            self.use_installed_proton()
        else:
            # Proton not installed or compatible, use bundled proton-c sources
            self.use_bundled_proton()
        super().build_extensions()


setup(name='python-qpid-proton',
      cmdclass={
          'build_ext': BuildExtension
      },
      # Note well: the following extension instance is modified during the
      # installation!  If you make changes below, you may need to update the
      # Configure class above
      ext_modules=[Extension('_cproton',
                             sources=['cprotonPYTHON_wrap.c'],
                             libraries=['qpid-proton-core'])])
