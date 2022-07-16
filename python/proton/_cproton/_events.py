from _proton_core import ffi, lib

from _proton_core.lib import PN_CONNECTION_BOUND, PN_CONNECTION_FINAL, PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_CLOSE, \
    PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_REMOTE_CLOSE, PN_CONNECTION_REMOTE_OPEN, PN_CONNECTION_UNBOUND, PN_DELIVERY, \
    PN_LINK_FINAL, PN_LINK_FLOW, PN_LINK_INIT, PN_LINK_LOCAL_CLOSE, PN_LINK_LOCAL_DETACH, PN_LINK_LOCAL_OPEN, \
    PN_LINK_REMOTE_CLOSE, PN_LINK_REMOTE_DETACH, PN_LINK_REMOTE_OPEN, PN_SESSION_FINAL, PN_SESSION_INIT, \
    PN_SESSION_LOCAL_CLOSE, PN_SESSION_LOCAL_OPEN, PN_SESSION_REMOTE_CLOSE, PN_SESSION_REMOTE_OPEN, PN_TIMER_TASK, \
    PN_TRANSPORT, PN_TRANSPORT_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_TAIL_CLOSED, \
    pn_cast_pn_connection, pn_cast_pn_delivery, pn_cast_pn_link, pn_cast_pn_session, pn_cast_pn_transport, \
    pn_class_name, pn_collector, pn_collector_free, pn_collector_more, pn_collector_peek, pn_collector_pop, \
    pn_collector_put, pn_collector_release, pn_event_class, pn_event_connection, pn_event_context, pn_event_delivery, \
    pn_event_link, pn_event_session, pn_event_transport, pn_event_type, pn_event_type_name

# this is something that swig gives for free
# how best to do this part of api translation automatically, everywhere?
# same thing for strings, etc.

"""
can get all functions inside "lib" and pretty much generate almost all these methods if I can get at signatures

"""


def _optional(value):
    if value == ffi.NULL:
        return None
    return value


def pn_class_name(cls):
    return ffi.string(lib.pn_class_name(cls)).decode("utf8")


def pn_event_link(event):
    return _optional(lib.pn_event_link(event))


def pn_event_session(event):
    return _optional(lib.pn_event_session(event))


def pn_event_connection(event):
    return _optional(lib.pn_event_connection(event))


def pn_event_transport(event):
    return _optional(lib.pn_event_transport(event))


# def pn_event_class(event):
#     return _optional(lib.pn_event_class(event))


def pn_collector_peek(collector):
    return _optional(lib.pn_collector_peek(collector))
