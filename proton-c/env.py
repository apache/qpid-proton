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
#

#
# A platform-agnostic tool for running a program in a modified environment.
#

import sys
import os
import subprocess
from optparse import OptionParser

def main(argv=None):

    parser = OptionParser(usage="Usage: %prog [options] [--] VAR=VALUE... command [options] arg1 arg2...")
    parser.add_option("-i", "--ignore-environment",
                      action="store_true", default=False,
                      help="Start with an empty environment (do not inherit current environment)")

    (options, args) = parser.parse_args(args=argv)

    if options.ignore_environment:
        new_env = {}
    else:
        new_env = os.environ.copy()

    # pull out each name value pair
    while (len(args)):
        z = args[0].split("=",1)
        if len(z) != 2:
            break;  # done with env args
        if len(z[0]) == 0:
            raise Exception("Error: incorrect format for env var: '%s'" % str(args[x]))
        del args[0]
        if len(z[1]) == 0:
            # value is not present, so delete it
            if z[0] in new_env:
                del new_env[z[0]]
        else:
            new_env[z[0]] = z[1]

    if len(args) == 0 or len(args[0]) == 0:
        raise Exception("Error: syntax error in command arguments")

    p = subprocess.Popen(args, env=new_env)
    return p.wait()

if __name__ == "__main__":
    sys.exit(main())
