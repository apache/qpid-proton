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



def pkg_config_version_installed(package, version=None, atleast=None):
    """Check if version of a package is is installed

    This function returns True/False depending on whether
    the package is found and is the correct version.

    :param version: The exact version of the package required
    :param atleast: True if installed package is at least this version
    """

    if version is None and atleast is None:
        log.fatal('package version string required')
    elif version and atleast:
        log.fatal('Specify either version or atleast, not both')

    check = 'exact' if version else 'atleast'
    p = _call_pkg_config(['--%s-version=%s' % (check,
                                               version or atleast),
                          package])
    if p:
        out,err = p.communicate()
        if p.returncode:
            log.info("Did not find %s via pkg-config: %s" % (package, err))
            return False
        log.info("Using %s version %s (found via pkg-config)" %
                 (package,
                  _call_pkg_config(['--modversion', package]).communicate()[0].splitlines()[0]))
        return True
    return False


def pkg_config_get_var(package, name):
    """Retrieve the value of the named package variable as a string
    """
    p = _call_pkg_config(['--variable=%s' % name, package])
    if not p:
        log.warn("pkg-config: var %s get failed, package %s", name, package)
        return ""
    out,err = p.communicate()
    if p.returncode:
        out = ""
        log.warn(err)
    return out.splitlines()[0]

