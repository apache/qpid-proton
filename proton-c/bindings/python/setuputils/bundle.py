#-----------------------------------------------------------------------------
#  Copyright (C) PyZMQ Developers
#  Distributed under the terms of the Modified BSD License.
#
#  This bundling code is largely adapted from pyzmq-static's get.sh by
#  Brandon Craig-Rhodes, which is itself BSD licensed.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
#  This bundling code is largely adapted from pyzmq's code
#  PyZMQ Developers, which is itself Modified BSD licensed.
#-----------------------------------------------------------------------------

import os
import shutil
import stat
import sys
import tarfile
from glob import glob
from subprocess import Popen, PIPE

try:
    # py2
    from urllib2 import urlopen
except ImportError:
    # py3
    from urllib.request import urlopen

from . import log


#-----------------------------------------------------------------------------
# Constants
#-----------------------------------------------------------------------------
min_qpid_proton = (0, 11, 0)
min_qpid_proton_str = "%i.%i.%i" % min_qpid_proton

bundled_version = (0, 11, 1)
bundled_version_str = "%i.%i.%i" % bundled_version
libqpid_proton = "qpid-proton-%s.tar.gz" % bundled_version_str
libqpid_proton_url = ("http://www.apache.org/dist/qpid/proton/%s/%s" %
                      (bundled_version_str, libqpid_proton))

HERE = os.path.dirname(__file__)
ROOT = os.path.dirname(HERE)


def fetch_archive(savedir, url, fname):
    """Download an archive to a specific location

    :param savedir: Destination dir
    :param url: URL where the archive should be downloaded from
    :param fname: Archive's filename
    """
    dest = os.path.join(savedir, fname)

    if os.path.exists(dest):
        log.info("already have %s" % fname)
        return dest

    log.info("fetching %s into %s" % (url, savedir))
    if not os.path.exists(savedir):
        os.makedirs(savedir)
    req = urlopen(url)
    with open(dest, 'wb') as f:
        f.write(req.read())
    return dest


def fetch_libqpid_proton(savedir):
    """Download qpid-proton to `savedir`."""
    def _c_only(members):
        # just extract the files necessary to build the shared library
        for tarinfo in members:
            npath = os.path.normpath(tarinfo.name)
            if ("proton-c/src" in npath or
                "proton-c/include" in npath or
                "proton-c/mllib" in npath):
                yield tarinfo
    dest = os.path.join(savedir, 'qpid-proton')
    if os.path.exists(dest):
        log.info("already have %s" % dest)
        return
    fname = fetch_archive(savedir, libqpid_proton_url, libqpid_proton)
    tf = tarfile.open(fname)
    member = tf.firstmember.path
    if member == '.':
        member = tf.getmembers()[1].path
    with_version = os.path.join(savedir, member)
    tf.extractall(savedir, members=_c_only(tf))
    tf.close()
    shutil.move(with_version, dest)
