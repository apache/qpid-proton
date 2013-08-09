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
# Stop cmake build if pn_i_xxx substitute functions aren't used for
# the dangererous non-complying [v]snprintf family.  A source of
# painful bug-hunting.
#
# Each obj must be checked instead of just the dll since Visual Studio
# sometimes inserts references to vsnprintf in DllMainCRTStartup,
# causing false positives.
#
# bad: vsnprintf, __vsnprintf, _imp__vsnprintf, ...,  same for snprintf
# OK:  vsnprintf_s, pn_i_vsnprintf
#

import sys
import os
import subprocess
import glob
import re

def symcheck(objfile):

    symfile = objfile.replace('.obj', '.sym')
    cmd = ['dumpbin.exe', '/SYMBOLS', objfile, '/OUT:' + symfile]

    # /dev/null standin
    junk = open('junk', 'w')
    p = subprocess.Popen(cmd, stdout=junk)
    n = p.wait()
    if n != 0 :
        raise Exception("dumpbin call failure")

    f = open(symfile, 'r')
    for line in f :
        m = re.search(r'UNDEF.*\b([a-zA-Z_]*snprintf)\b', line)
        if m :
            sym = m.group(1)
            if re.match(r'_*pn_i_v?snprintf', sym) is None :
                raise Exception('Unsafe use of C99 violating function in  ' + objfile + ' : ' + sym)

def main():
    os.chdir(sys.argv[1])
    objs = glob.glob('*.obj')
    for obj in glob.glob('*.obj'):
        symcheck(obj)

if __name__ == "__main__":
    sys.exit(main())
