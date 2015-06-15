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


def pkg_config_version(atleast=None, max_version=None, module='libqpid-proton'):
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
               module]
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

    log.info("Using %s (found via pkg-config)." % module)
    return True
