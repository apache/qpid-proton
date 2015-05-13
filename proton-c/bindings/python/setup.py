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

import os
import subprocess
import sys

import distutils.spawn as ds_spawn
import distutils.sysconfig as ds_sys
from distutils.core import setup, Extension
from distutils.command.build import build
from distutils.command.build_ext import build_ext

from setuputils import bundle
from setuputils import log
from setuputils import misc


class CustomBuildOrder(build):
    # NOTE(flaper87): The sole purpose of this class is to re-order
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

    def build_extensions(self):
        self.check_extensions_list(self.extensions)

        for ext in self.extensions:
            self.build_extension(ext)

    def build_cmake_proton(self, ext):
        if not ds_spawn.find_executable('cmake'):
            log.fatal("cmake needed for this script it work")

        try:
            ds_spawn.spawn(['cmake'] + ext.extra_compile_args + ext.sources)
            ds_spawn.spawn(['make', 'install'])
        except ds_spawn.DistutilsExecError:
            msg = "Error while running cmake"
            msg += "\nrun 'setup.py build --help' for build options"
            log.fatal(msg)

    def build_extension(self, ext):
        if ext.name == 'cmake_cproton':
            return self.build_cmake_proton(ext)

        return build_ext.build_extension(self, ext)

    def run(self):
        # Discover qpid-proton in the system
        self.distribution.run_command('configure')
        build_ext.run(self)


class Configure(build_ext):
    description = "Discover Qpid Proton version"

    def bundle_libqpid_proton_extension(self):
        bundledir = "bundled"
        ext_modules = self.distribution.ext_modules

        log.info("Using bundled libqpid-proton")

        if not os.path.exists(bundledir):
            os.makedirs(bundledir)

        bundle.fetch_libqpid_proton(bundledir)

        libqpid_proton_dir = os.path.join(bundledir, 'qpid-proton')

        # NOTE(flaper87): Find prefix. We need to run make ourselves
        libqpid_proton = Extension(
            'cmake_cproton',
            sources = [libqpid_proton_dir],

            # NOTE(flaper87): Disable all bindings, set the prefix to whatever
            # is in `distutils.sysconfig.PREFIX` and finally, disable testing
            # as well. The python binding will be built by this script later
            # on. Don't let cmake do it.
            extra_compile_args = ['-DCMAKE_INSTALL_PREFIX:PATH=%s' % ds_sys.PREFIX,
                                  '-DBUILD_PYTHON=False',
                                  '-DBUILD_JAVA=False',
                                  '-DBUILD_PERL=False',
                                  '-DBUILD_RUBY=False',
                                  '-DBUILD_PHP=False',
                                  '-DBUILD_JAVASCRIPT=False',
                                  '-DBUILD_TESTING=False',
            ]
        )

        # NOTE(flaper87): Register this new extension and make
        # sure it's built and installed *before* `_cproton`.
        self.distribution.ext_modules.insert(0, libqpid_proton)

    def set_cproton_settings(self, prefix=None):
        settings = misc.settings_from_prefix(prefix)
        ext = self.distribution.ext_modules[-1]

        for k, v in settings.items():
            setattr(ext, k, v)

    def check_qpid_proton_version(self):
        """check the qpid_proton version"""

        target_version = bundle.bundled_version_str
        return (misc.pkg_config_version(max_version=target_version) and
                misc.pkg_config_version(atleast=bundle.min_qpid_proton_str))


    @property
    def bundle_proton(self):
        return sys.platform == 'linux2' and not self.check_qpid_proton_version()

    def run(self):
        prefix = None
        if self.bundle_proton:
            self.bundle_libqpid_proton_extension()
            prefix = ds_sys.PREFIX

        self.set_cproton_settings(prefix)


# NOTE(flaper87): Override `build_ext` and add `configure`
cmdclass = {'configure': Configure,
            'build': CustomBuildOrder,
            'build_ext': CheckingBuildExt}


setup(name='python-qpid-proton',
      version=bundle.min_qpid_proton_str,
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
                             libraries=['qpid-proton'])])
