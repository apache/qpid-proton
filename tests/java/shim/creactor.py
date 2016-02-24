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
from proton import _compat
from cerror import Skipped
from cengine import wrap, pn_connection_wrapper

from org.apache.qpid.proton.reactor import Reactor
from org.apache.qpid.proton.engine import BaseHandler, HandlerException

# from proton/reactor.h
def pn_reactor():
    return Reactor.Factory.create()
def pn_reactor_attachments(r):
    return r.attachments()
def pn_reactor_get_global_handler(r):
    return r.getGlobalHandler()
def pn_reactor_set_global_handler(r, h):
    r.setGlobalHandler(h)
def pn_reactor_get_handler(r):
    return r.getHandler()
def pn_reactor_set_handler(r, h):
    r.setHandler(h)
def pn_reactor_set_timeout(r, t):
    r.setTimeout(t)
def pn_reactor_get_timeout(r):
    return r.getTimeout()
def pn_reactor_schedule(r, t, h):
    return r.schedule(t, h)
def pn_reactor_yield(r):
    getattr(r, "yield")()
def pn_reactor_start(r):
    r.start()
def pn_reactor_process(r):
    return peel_handler_exception(r.process)
def pn_reactor_stop(r):
    return peel_handler_exception(r.stop)
def pn_reactor_selectable(r):
    return r.selectable()
def pn_reactor_connection(r, h):
    return wrap(r.connection(h), pn_connection_wrapper)
def pn_reactor_acceptor(r, host, port, handler):
    return r.acceptor(host, int(port), handler)
def pn_reactor_mark(r):
    return r.mark()
def pn_reactor_wakeup(r):
    return r.wakeup()

def peel_handler_exception(meth):
    try:
        return meth()
    except HandlerException, he:
        cause = he.cause
        t = getattr(cause, "type", cause.__class__)
        info = sys.exc_info()
        _compat.raise_(t, cause, info[2]) 

def pn_handler_add(h, c):
    h.add(c)
def pn_handler_dispatch(h, ev, et):
    if et != None and et != ev.impl.type:
      ev.impl.redispatch(et, h)
    else:
      ev.impl.dispatch(h)
def pn_record_set_handler(r, h):
    BaseHandler.setHandler(r, h)
def pn_record_get_handler(r):
    return BaseHandler.getHandler(r)

def pn_task_attachments(t):
    return t.attachments()

def pn_selectable_attachments(s):
    return s.attachments()

def pn_selectable_set_fd(s, fd):
    s.setChannel(fd.getChannel())

def pn_acceptor_close(a):
    a.close()

def pn_task_cancel(t):
    t.cancel()

def pn_object_reactor(o):
    if hasattr(o, "impl"):
        if hasattr(o.impl, "getSession"):
            return o.impl.getSession().getConnection().getReactor()
        elif hasattr(o.impl, "getConnection"):
            return o.impl.getConnection().getReactor()
        else:
            return o.impl.getReactor()
    else:
        return o.getReactor()
