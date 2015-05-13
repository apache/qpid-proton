#-----------------------------------------------------------------------------
#  Copyright (C) PyZMQ Developers
#  Distributed under the terms of the Modified BSD License.
#
#  This bundling code is largely adapted from pyzmq-static's get.sh by
#  Brandon Craig-Rhodes, which is itself BSD licensed.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
#  These functions were largely adapted from pyzmq's code
#  PyZMQ Developers, which is itself Modified BSD licensed.
#-----------------------------------------------------------------------------


import os
import subprocess
import sys

from . import log


def settings_from_prefix(prefix=None):
    """Load appropriate library/include settings from qpid-proton prefix

    NOTE: Funcion adapted from PyZMQ, which is itself Modified BSD licensed.

    :param prefix: The install prefix where the dependencies are expected to be
    """
    settings = {}
    settings['libraries'] = []
    settings['include_dirs'] = []
    settings['library_dirs'] = []
    settings['swig_opts'] = []
    settings['runtime_library_dirs'] = []
    settings['extra_link_args'] = []

    if sys.platform.startswith('win'):
        settings['libraries'].append('libqpid-proton')

        if prefix:
            settings['include_dirs'] += [os.path.join(prefix, 'include')]
            settings['library_dirs'] += [os.path.join(prefix, 'lib')]
    else:

        # If prefix is not explicitly set, pull it from pkg-config by default.

        if not prefix:
            try:
                cmd = 'pkg-config --variable=prefix --print-errors libqpid-proton'.split()
                p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            except OSError as e:
                if e.errno == errno.ENOENT:
                    log.info("pkg-config not found")
                else:
                    log.warn("Running pkg-config failed - %s." % e)
                p = None
            if p is not None:
                if p.wait():
                    log.info("Did not find libqpid-proton via pkg-config:")
                    log.info(p.stderr.read().decode())
                else:
                    prefix = p.stdout.readline().strip().decode()
                    log.info("Using qpid-proton-prefix %s (found via pkg-config)." % prefix)

        settings['libraries'].append('qpid-proton')
        # add pthread on freebsd
        if sys.platform.startswith('freebsd'):
            settings['libraries'].append('pthread')

        if sys.platform == 'sunos5':
          if platform.architecture()[0] == '32bit':
            settings['extra_link_args'] += ['-m32']
          else:
            settings['extra_link_args'] += ['-m64']
        if prefix:
            settings['include_dirs'] += [os.path.join(prefix, 'include')]
            settings['library_dirs'] += [os.path.join(prefix, 'lib')]
            settings['library_dirs'] += [os.path.join(prefix, 'lib64')]

        else:
            if sys.platform == 'darwin' and os.path.isdir('/opt/local/lib'):
                # allow macports default
                settings['include_dirs'] += ['/opt/local/include']
                settings['library_dirs'] += ['/opt/local/lib']
                settings['library_dirs'] += ['/opt/local/lib64']
            if os.environ.get('VIRTUAL_ENV', None):
                # find libqpid_proton installed in virtualenv
                env = os.environ['VIRTUAL_ENV']
                settings['include_dirs'] += [os.path.join(env, 'include')]
                settings['library_dirs'] += [os.path.join(env, 'lib')]
                settings['library_dirs'] += [os.path.join(env, 'lib64')]

        if sys.platform != 'darwin':
            settings['runtime_library_dirs'] += [
                os.path.abspath(x) for x in settings['library_dirs']
            ]

    for d in settings['include_dirs']:
        settings['swig_opts'] += ['-I' + d]

    return settings


def pkg_config_version(atleast=None, max_version=None):
    """Check the qpid_proton version using pkg-config

    This function returns True/False depending on whether
    the library is found and atleast/max_version are met.

    :param atleast: The minimum required version
    :param max_version: The maximum supported version. This
        basically represents the target version.
    """

    if atleast and max_version:
        log.error('Specify either atleast or max_version')

    try:
        cmd = ['pkg-config',
               '--%s-version=%s' % (atleast and 'atleast' or 'max',
                                    atleast or max_version),
               'libqpid-proton']
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError as e:
        if e.errno == errno.ENOENT:
            log.info("pkg-config not found")
        else:
            log.warn("Running pkg-config failed - %s." % e)
        return False

    if p.wait():
        log.info("Did not find libqpid-proton via pkg-config:")
        log.info(p.stderr.read().decode())
        return False

    log.info("Using libqpid-proton (found via pkg-config).")
    return True
