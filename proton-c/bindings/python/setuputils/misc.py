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


import errno
import os
import subprocess
import sys

from . import log

def _call_pkg_config(args):
    """Spawn a subprocess running pkg-config with the given args.

    :param args: list of strings to pass to pkg-config's command line.
    Refer to pkg-config's documentation for more detail.

    Return the Popen object, or None if the command failed
    """
    try:
        return subprocess.Popen(['pkg-config'] + args,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                universal_newlines=True)
    except OSError as e:
        if e.errno == errno.ENOENT:
            log.warn("command not found: pkg-config")
        else:
            log.warn("Running pkg-config failed - %s." % e)
    return None



def pkg_config_version(atleast=None, max_version=None, module='libqpid-proton'):
    """Check the qpid_proton version using pkg-config

    This function returns True/False depending on whether
    the library is found and atleast/max_version are met.

    :param atleast: The minimum required version
    :param max_version: The maximum supported version. This
        basically represents the target version.
    """

    if atleast and max_version:
        log.fatal('Specify either atleast or max_version')

    p = _call_pkg_config(['--%s-version=%s' % (atleast and 'atleast' or 'max',
                                               atleast or max_version),
                          module])
    if p:
        out,err = p.communicate()
        if p.returncode:
            log.info("Did not find %s via pkg-config: %s" % (module, err))
            return False
        log.info("Using %s (found via pkg-config)." % module)
        return True
    return False


def pkg_config_get_var(name, module='libqpid-proton'):
    """Retrieve the value of the named module variable as a string
    """
    p = _call_pkg_config(['--variable=%s' % name, module])
    if not p:
        log.warn("pkg-config: var %s get failed, package %s", name, module)
        return ""
    out,err = p.communicate()
    if p.returncode:
        out = ""
        log.warn(err)
    return out

