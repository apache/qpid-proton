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

import sys
from cerror import Skipped
from org.apache.qpid.proton.reactor import FlowController, Handshaker
from org.apache.qpid.proton.engine import BaseHandler, HandlerException

# from proton/handlers.h
def pn_flowcontroller(window):
    return FlowController(window)

def pn_handshaker():
    return Handshaker()

def pn_iohandler():
    raise Skipped()

from cengine import pn_event, pn_event_type

class pn_pyhandler(BaseHandler):

    def __init__(self, pyobj):
        self.pyobj = pyobj

    def onUnhandled(self, event):
        ev = pn_event(event)
        try:
            self.pyobj.dispatch(ev, pn_event_type(ev))
        except HandlerException:
            ex = sys.exc_info();
            cause = ex[1].cause
            if hasattr(cause, "value"):
                cause = cause.value
            t = type(cause)
            self.pyobj.exception(t, cause, ex[2])
        except:
            ex = sys.exc_info()
            self.pyobj.exception(*ex)
