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

The setup script will attempt to download the C source for qpid-proton - see
`setuputils.bundle.fetch_libqpid_proton` - and it will include the proton C
code into the extension itself.

While the above removes the need of *always* having qpid-proton installed, it
does not solve the need of having `swig` and the libraries qpid-proton requires
installed to make this setup work.

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
from distutils.command.sdist import sdist
from distutils import errors

from setuputils import bundle
from setuputils import log
from setuputils import misc


class CheckSDist(sdist):

    def run(self):
        self.distribution.run_command('configure')

        # Append the source that was removed during
        # the configuration step.
        _cproton = self.distribution.ext_modules[-1]
        _cproton.sources.append('cproton.i')

        try:
            sdist.run(self)
        finally:
            for src in ['cproton.py', 'cproton_wrap.c']:
                if os.path.exists(src):
                    os.remove(src)


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

    def prepare_swig_wrap(self):
        """Run swig against the sources.  This will cause swig to compile the
        cproton.i file into a .c file called cproton_wrap.c, and create
        cproton.py.
        """
        ext = self.distribution.ext_modules[-1]

        if 'SWIG' in os.environ:
            self.swig = os.environ['SWIG']

        try:
            # This will actually call swig to generate the files
            # and list the sources.
            self.swig_sources(ext.sources, ext)
        except (errors.DistutilsExecError, errors.DistutilsPlatformError) as e:
            if not (os.path.exists('cproton_wrap.c') or
                    os.path.exists('cproton.py')):
                raise e

        # now remove the cproton.i file from the source list so we don't run
        # swig again.
        ext.sources = ext.sources[1:]
        ext.swig_opts = []

    def bundle_libqpid_proton_extension(self):
        """The proper version of libqpid-proton is not present on the system,
        so attempt to retrieve the proper libqpid-proton sources and
        include them in the extension.
        """
        setup_path = os.path.dirname(os.path.realpath(__file__))
        base = self.get_finalized_command('build').build_base
        build_include = os.path.join(base, 'include')

        log.info("Bundling qpid-proton into the extension")

        # QPID_PROTON_SRC - (optional) pathname to the Proton C sources.  Can
        # be used to override where this setup gets the Proton C sources from
        # (see bundle.fetch_libqpid_proton())
        if 'QPID_PROTON_SRC' not in os.environ:
            if not os.path.exists(os.path.join(setup_path, 'tox.ini')):
                bundledir = os.path.join(base, "bundled")
                if not os.path.exists(bundledir):
                    os.makedirs(bundledir)
                bundle.fetch_libqpid_proton(bundledir)
                libqpid_proton_dir = os.path.abspath(os.path.join(bundledir, 'qpid-proton'))
            else:
                # This should happen just in **dev** environemnts since
                # tox.ini is not shipped with the driver. It should only
                # be triggered when calling `setup.py`. This can happen either
                # manually or when calling `tox` in the **sdist** step. Tox will
                # defined the `QPID_PROTON_SRC` itself.
                proton_c = os.path.join(setup_path, '..', '..', '..')
                libqpid_proton_dir = os.path.abspath(proton_c)
        else:
            libqpid_proton_dir = os.path.abspath(os.environ['QPID_PROTON_SRC'])

        log.debug("Using libqpid-proton src: %s" % libqpid_proton_dir)

        proton_base = os.path.join(libqpid_proton_dir, 'proton-c')
        proton_src = os.path.join(proton_base, 'src')
        proton_include = os.path.join(proton_base, 'include')

        #
        # Create any generated header files, and put them in build_include:
        #
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
        # major and minor versions from the downloaded tarball. This version should
        # match the ones in the bundle module
        with open(os.path.join(build_include, 'proton', 'version.h'), "wb") as ver:
            version_text = """
#ifndef _PROTON_VERSION_H
#define _PROTON_VERSION_H 1
#define PN_VERSION_MAJOR %i
#define PN_VERSION_MINOR %i
#define PN_VERSION_POINT %i
#endif /* version.h */
""" % bundle.bundled_version
            ver.write(version_text.encode('utf-8'))

        # Collect all the Proton C files that need to be built.
        # we could've used `glob(.., '*', '*.c')` but I preferred going
        # with an explicit list of subdirs that we can control and expand
        # depending on the version. Specifically, lets avoid adding things
        # we don't need.

        sources = []
        for subdir in ['object', 'framing', 'codec', 'dispatcher',
                       'engine', 'events', 'transport',
                       'message', 'reactor', 'messenger',
                       'handlers', 'posix']:

            sources.extend(glob.glob(os.path.join(proton_src, subdir, '*.c')))

        sources.extend(filter(lambda x: not x.endswith('dump.c'),
                       glob.iglob(os.path.join(proton_src, '*.c'))))

        # Look for any optional libraries that proton needs, and adjust the
        # source list and compile flags as necessary.
        libraries = []

        # -D flags (None means no value, just define)
        macros=[('qpid_proton_EXPORTS', None),
                ('USE_ATOLL', None),
                ('USE_STRERROR_R', None)]

        # Check whether openssl is installed by poking
        # pkg-config for a minimum version 0. If it's installed, it should
        # return True and we'll use it. Otherwise, we'll use the stub.
        if misc.pkg_config_version(atleast='0', module='openssl'):
            libraries += ['ssl', 'crypto']
            sources.append(os.path.join(proton_src, 'ssl', 'openssl.c'))
        else:
            sources.append(os.path.join(proton_src, 'ssl', 'ssl_stub.c'))

        # create a temp compiler to check for optional compile-time features
        cc = new_compiler(compiler=self.compiler_type)
        cc.output_dir = self.build_temp

        # Some systems need to link to `rt`. Check whether `clock_gettime` is
        # around and if librt is needed
        if cc.has_function('clock_gettime'):
            macros.append(('USE_CLOCK_GETTIME', None))
        else:
            if cc.has_function('clock_gettime', libraries=['rt']):
                libraries.append('rt')
                macros.append(('USE_CLOCK_GETTIME', None))

        # 0.10 added an implementation for cyrus. Check
        # if it is available before adding the implementation to the sources
        # list. Eventually, `sasl.c` will be added and one of the existing
        # implementations will be used.
        if cc.has_function('sasl_client_done', includes=['sasl/sasl.h'],
                           libraries=['sasl2']):
            libraries.append('sasl2')
            sources.append(os.path.join(proton_src, 'sasl', 'cyrus_sasl.c'))
        else:
            sources.append(os.path.join(proton_src, 'sasl', 'none_sasl.c'))

        sources.append(os.path.join(proton_src, 'sasl', 'sasl.c'))

        # compile all the proton sources.  We'll add the resulting list of
        # objects to the _cproton extension as 'extra objects'.  We do this
        # instead of just lumping all the sources into the extension to prevent
        # any proton-specific compilation flags from affecting the compilation
        # of the generated swig code

        cc = new_compiler(compiler=self.compiler_type)
        ds_sys.customize_compiler(cc)

        objects = cc.compile(sources,
                             macros=macros,
                             include_dirs=[build_include,
                                           proton_include,
                                           proton_src],
                             # compiler command line options:
                             extra_postargs=['-std=gnu99'],
                             output_dir=self.build_temp)

        #
        # Now update the _cproton extension instance to include the objects and
        # libraries
        #
        _cproton = self.distribution.ext_modules[-1]
        _cproton.extra_objects = objects
        _cproton.include_dirs.append(build_include)
        _cproton.include_dirs.append(proton_include)

        # swig will need to access the proton headers:
        _cproton.swig_opts.append('-I%s' % build_include)
        _cproton.swig_opts.append('-I%s' % proton_include)

        # lastly replace the libqpid-proton dependency with libraries required
        # by the Proton objects:
        _cproton.libraries=libraries

    def check_qpid_proton_version(self):
        """check the qpid_proton version"""

        target_version = bundle.bundled_version_str
        return (misc.pkg_config_version(max_version=target_version) and
                misc.pkg_config_version(atleast=bundle.min_qpid_proton_str))

    @property
    def bundle_proton(self):
        """Need to bundle proton if the conditions below are met."""
        return ('QPID_PROTON_SRC' in os.environ) or \
            (not self.check_qpid_proton_version())

    def use_installed_proton(self):
        """The Proton development headers and library are installed, update the
        _cproton extension to tell it where to find the library and headers.
        """
        _cproton = self.distribution.ext_modules[-1]
        incs = misc.pkg_config_get_var('includedir')
        for i in incs.split():
            _cproton.swig_opts.append('-I%s' % i)
            _cproton.include_dirs.append(i)
        ldirs = misc.pkg_config_get_var('libdir')
        _cproton.library_dirs.extend(ldirs.split())

    def run(self):
        # check if the Proton library and headers are installed and are
        # compatible with this version of the binding.
        if self.bundle_proton:
            # Proton not installed or compatible
            self.bundle_libqpid_proton_extension()
        else:
            self.use_installed_proton()
        self.prepare_swig_wrap()


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
            'build_ext': CheckingBuildExt,
            'sdist': CheckSDist}

setup(name='python-qpid-proton',
      version=bundle.bundled_version_str + ".post1",
      description='An AMQP based messaging library.',
      author='Apache Qpid',
      author_email='proton@qpid.apache.org',
      url='http://qpid.apache.org/proton/',
      packages=['proton'],
      py_modules=['cproton'],
      license="Apache Software License",
      classifiers=["License :: OSI Approved :: Apache Software License",
                   "Intended Audience :: Developers",
                   "Programming Language :: Python",
                   "Programming Language :: Python :: 2",
                   "Programming Language :: Python :: 2.6",
                   "Programming Language :: Python :: 2.7",
                   "Programming Language :: Python :: 3",
                   "Programming Language :: Python :: 3.3",
                   "Programming Language :: Python :: 3.4",
                   "Programming Language :: Python :: 3.5"],
      cmdclass=cmdclass,
      # Note well: the following extension instance is modified during the
      # installation!  If you make changes below, you may need to update the
      # Configure class above
      ext_modules=[Extension('_cproton',
                             sources=['cproton.i', 'cproton_wrap.c'],
                             swig_opts=['-threads'],
                             extra_compile_args=['-pthread'],
                             libraries=['qpid-proton'])])
