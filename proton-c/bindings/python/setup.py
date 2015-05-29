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

The default behavior of this script is to build the registered `_cproton`
extension using the system qpid-proton library. However, before doing that, the
script will try to discover the available qpid-proton's version and whether it's
suitable for the version of this library. This allows us to have a tight release
between the versions of the bindings and qpid-proton's version.

The versions used to verify this are in `setuputils.bundle` and they should be
increased on every release. Note that `bundled_version` matches the current
released version. The motivation behind this is that we don't know how many
new releases will be made in the `0.9` series, therefore we need to target the
latest possible.

If qpid-proton is found in the system and the available versions are match the
required ones, then the install process will continue normally.

If the available versions are not good for the bindings or the library is
missing, then the following will happen:

The setup script will attempt to download qpid-proton's tar - see
`setuputils.bundle.fetch_libqpid_proton` - and it'll build it and then install
it. Note that if this is being executed outside a virtualenv, it'll install it
on whatever `distutils.sysconfig.PREFIX` is available. Once qpid-proton has been
built and installed, the extension build will proceed normally as if the library
would have been found. The extension will use the recently built library.

While the above removes the need of *always* having qpid-proton installed, it does
not solve the need of having `cmake` and `swig` installed to make this setup work.
Eventually, future works should remove the need of `cmake` by collecting sources
and letting `distutils` do the compilation.

On a final note, it's important to say that in either case, the library paths will
be added as `rpaths` to the `_cproton` shared library. Mainly because we need to
make sure `libqpid-proton.so` will be found and loaded when it's not installed in
the main system.

From the Python side, this scripts overrides 1 command - build_ext - and it adds a
new one. The later - Configure - is called from the former to setup/discover what's
in the system. The rest of the comands and steps are done normally without any kind
of monkey patching.
"""

import glob
import os
import subprocess
import sys

import distutils.spawn as ds_spawn
import distutils.sysconfig as ds_sys
from distutils.ccompiler import new_compiler, get_default_compiler
from distutils.core import setup, Extension
from distutils.command.build import build
from distutils.command.build_ext import build_ext

from setuputils import bundle
from setuputils import log
from setuputils import misc


class Configure(build_ext):
    description = "Discover Qpid Proton version"

    @property
    def compiler_type(self):
        compiler = self.compiler
        if compiler is None:
            return get_default_compiler()
        elif isinstance(compiler, str):
            return compiler
        else:
            return compiler.compiler_type

    def bundle_libqpid_proton_extension(self):
        base = self.get_finalized_command('build').build_base
        build_include = os.path.join(base, 'include')
        install = self.get_finalized_command('install').install_base
        install_lib = self.get_finalized_command('install').install_lib
        ext_modules = self.distribution.ext_modules

        log.info("Using bundled libqpid-proton")

        if 'QPID_PROTON_SRC' not in os.environ:
            bundledir = os.path.join(base, "bundled")
            if not os.path.exists(bundledir):
                os.makedirs(bundledir)
            bundle.fetch_libqpid_proton(bundledir)
            libqpid_proton_dir = os.path.abspath(os.path.join(bundledir, 'qpid-proton'))
        else:
            libqpid_proton_dir = os.path.abspath(os.environ['QPID_PROTON_SRC'])

        log.debug("Using libqpid-proton src: %s" % libqpid_proton_dir)

        proton_base = os.path.join(libqpid_proton_dir, 'proton-c')
        proton_src = os.path.join(proton_base, 'src')
        proton_include = os.path.join(proton_base, 'include')

        if not os.path.exists(build_include):
            os.makedirs(build_include)
            os.mkdir(os.path.join(build_include, 'proton'))

        # Generate `protocol.h` by calling the python
        # script found in the source dir.
        with open(os.path.join(build_include, 'protocol.h'), 'wb') as header:
            subprocess.Popen([sys.executable, os.path.join(proton_src, 'protocol.h.py')],
                              env={'PYTHONPATH': proton_base}, stdout=header)

        # Generate `encodings.h` by calling the python
        # script found in the source dir.
        with open(os.path.join(build_include, 'encodings.h'), 'wb') as header:
            subprocess.Popen([sys.executable,
                              os.path.join(proton_src, 'codec', 'encodings.h.py')],
                              env={'PYTHONPATH': proton_base}, stdout=header)

        # Create a custom, temporary, version.h file mapping the
        # major and minor versions from the downloaded tarbal. This version should
        # match the ones in the bundle module
        with open(os.path.join(build_include, 'proton', 'version.h'), "wb") as ver:
            version_text = """
#ifndef _PROTON_VERSION_H
#define _PROTON_VERSION_H 1
#define PN_VERSION_MAJOR %i
#define PN_VERSION_MINOR %i
#endif /* version.h */
""" % bundle.min_qpid_proton
            ver.write(version_text)

        # Collect all the C files that need to be built.
        # we could've used `glob(.., '*', '*.c')` but I preferred going
        # with an explicit list of subdirs that we can control and expand
        # depending on the version. Specifically, lets avoid adding things
        # we don't need.
        sources = []
        libraries = ['uuid']

        for subdir in ['object', 'framing', 'codec', 'dispatcher',
                       'engine', 'events', 'transport',
                       'message', 'reactor', 'messenger',
                       'handlers', 'posix', 'sasl']:

            sources.extend(glob.glob(os.path.join(proton_src, subdir, '*.c')))

        sources.extend(filter(lambda x: not x.endswith('dump.c'),
                       glob.iglob(os.path.join(proton_src, '*.c'))))

        # Check whether openssl is installed by poking
        # pkg-config for a minimum version 0. If it's installed, it should
        # return True and we'll use it. Otherwise, we'll use the stub.
        if misc.pkg_config_version(atleast='0', module='openssl'):
            libraries += ['ssl', 'crypto']
            sources.append(os.path.join(proton_src, 'ssl', 'openssl.c'))
        else:
            sources.append(os.path.join(proton_src, 'ssl', 'ssl_stub.c'))

        cc = new_compiler(compiler=self.compiler_type)
        cc.output_dir = self.build_temp

        # Some systems need to link to
        # `rt`. Check whether `clock_getttime` is around
        # and if not, link on rt.
        if not cc.has_function('clock_getttime'):
            libraries.append('rt')

        # Create an extension for the bundled qpid-proton
        # library and let distutils do the build step for us.
        # This is not the `swig` library... What's going to be built by this
        # `Extension` is qpid-proton itself. For the swig library, pls, see the
        # dependencies in the `setup` function call and how it's extended further
        # down this method.
        libqpid_proton = Extension(
            'libqpid-proton',

            # List of `.c` files that will be compiled.
            # `sources` is defined earlier on in this method and it's mostly
            # filled dynamically except for a few cases where files are added
            # depending on the presence of some libraries.
            sources=sources,

            # Libraries that need to be linked to should
            # be added to this list. `libraries` is defined earlier on
            # in this same method and it's filled depending on some
            # conditions. You'll find comments on each of those.
            libraries=libraries,

            # Changes to this list should be rare.
            # However, this is where new headers' dirs are added.
            # This list translates to `-I....` flags.
            include_dirs=[build_include, proton_src, proton_include],

            # If you need to add a default flag, this is
            # the place. All these compile arguments will be appended to
            # the GCC command. This list of flags is not used during the
            # linking phase.
            extra_compile_args = [
                '-std=gnu99',
                '-Dqpid_proton_EXPORTS',
                '-DUSE_ATOLL',
                '-DUSE_CLOCK_GETTIME',
                '-DUSE_STRERROR_R',
                '-DUSE_UUID_GENERATE',
            ],

            # If you need to add flags to the linking phase
            # this is the right place to do it. Just like the compile flags,
            # this is a list of flags that will be appended to the link
            # command.
            extra_link_args = []
        )


        # Extend the `swig` module `Extension` and add a few
        # extra options. For instance, we'll add new library dirs where `swig`
        # should look for headers and libraries. In addition to this, we'll
        # also append a `runtime path` where the qpid-proton library for this
        # swig extension should be looked up from whenever the proton bindings
        # are imported. We need this because the library will live in the
        # site-packages along with the proton bindings instead of being in the
        # common places like `/usr/lib` or `/usr/local/lib`.
        #
        # This is not the place where you'd add "default" flags. If you need to
        # add flags like `-thread` please read the `setup` function call at the
        # bottom of this file and see the examples there.
        _cproton = self.distribution.ext_modules[-1]
        _cproton.library_dirs.append(self.build_lib)
        _cproton.include_dirs.append(proton_include)
        _cproton.include_dirs.append(os.path.join(proton_src, 'bindings', 'python'))

        _cproton.swig_opts.append('-I%s' % build_include)
        _cproton.swig_opts.append('-I%s' % proton_include)

        _cproton.runtime_library_dirs.extend([install_lib])

        # Register this new extension and make
        # sure it's built and installed *before* `_cproton`.
        self.distribution.ext_modules.insert(0, libqpid_proton)

    def check_qpid_proton_version(self):
        """check the qpid_proton version"""

        target_version = bundle.bundled_version_str
        return (misc.pkg_config_version(max_version=target_version) and
                misc.pkg_config_version(atleast=bundle.min_qpid_proton_str))

    @property
    def bundle_proton(self):
        """Bundled proton if the conditions below are met."""
        return sys.platform == 'linux2' and not self.check_qpid_proton_version()

    def run(self):
        if self.bundle_proton:
            self.bundle_libqpid_proton_extension()


class CustomBuildOrder(build):
    # The sole purpose of this class is to re-order
    # the commands execution so that `build_ext` is executed *before*
    # build_py. We need this to make sure `cproton.py` is generated
    # before the python modules are collected. Otherwise, it won't
    # be installed.
    sub_commands = [
        ('build_ext', build.has_ext_modules),
        ('build_py', build.has_pure_modules),
        ('build_clib', build.has_c_libraries),
        ('build_scripts', build.has_scripts),
    ]


class CheckingBuildExt(build_ext):
    """Subclass build_ext to build qpid-proton using `cmake`"""

    def run(self):
        # Discover qpid-proton in the system
        self.distribution.run_command('configure')
        build_ext.run(self)


# Override `build_ext` and add `configure`
cmdclass = {'configure': Configure,
            'build': CustomBuildOrder,
            'build_ext': CheckingBuildExt}


setup(name='python-qpid-proton',
      version=bundle.bundled_version_str,
      description='An AMQP based messaging library.',
      author='Apache Qpid',
      author_email='proton@qpid.apache.org',
      url='http://qpid.apache.org/proton/',
      packages=['proton'],
      py_modules=['cproton'],
      license="Apache Software License",
      classifiers=["License :: OSI Approved :: Apache Software License",
                   "Intended Audience :: Developers",
                   "Programming Language :: Python"],
      cmdclass = cmdclass,
      ext_modules=[Extension('_cproton', ['cproton.i'],
                             swig_opts=['-threads'],
                             libraries=['qpid-proton'])])
