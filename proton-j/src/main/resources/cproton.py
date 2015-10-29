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

"""
The cproton module defines a java implementation of the C interface as
exposed to python via swig. This allows tests defined in python to run
against both the C and Java protocol implementations.
"""

# @todo(kgiusti) dynamically set these via filters in the pom.xml file
PN_VERSION_MAJOR = 0
PN_VERSION_MINOR = 0
PN_VERSION_POINT = 0

from ctypes import *
from cobject import *
from cerror import *
from ccodec import *
from cengine import *
from csasl import *
from cssl import *
from cdriver import *
from cmessenger import *
from cmessage import *
from curl import *
from creactor import *
from chandlers import *
