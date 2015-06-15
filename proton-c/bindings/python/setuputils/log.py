#-----------------------------------------------------------------------------
#  Copyright (C) PyZMQ Developers
#  Distributed under the terms of the Modified BSD License.
#
#  This bundling code is largely adapted from pyzmq-static's get.sh by
#  Brandon Craig-Rhodes, which is itself BSD licensed.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Logging (adapted from h5py: http://h5py.googlecode.com)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
#  This log code is largely adapted from pyzmq's code
#  PyZMQ Developers, which is itself Modified BSD licensed.
#-----------------------------------------------------------------------------


import os
import sys
import logging


logger = logging.getLogger()
if os.environ.get('DEBUG'):
    logger.setLevel(logging.DEBUG)
else:
    logger.setLevel(logging.INFO)
logger.addHandler(logging.StreamHandler(sys.stderr))


def debug(msg):
    logger.debug(msg)


def info(msg):
    logger.info(msg)


def fatal(msg, code=1):
    logger.error("Fatal: " + msg)
    exit(code)


def warn(msg):
    logger.error("Warning: " + msg)
