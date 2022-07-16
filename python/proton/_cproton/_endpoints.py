from _proton_core import ffi
from _proton_core import lib
from _proton_core.lib import PN_CONFIGURATION, PN_COORDINATOR, PN_DELIVERIES, PN_DIST_MODE_COPY, PN_DIST_MODE_MOVE, \
    PN_DIST_MODE_UNSPECIFIED, PN_EOS, PN_EXPIRE_NEVER, PN_EXPIRE_WITH_CONNECTION, PN_EXPIRE_WITH_LINK, \
    PN_EXPIRE_WITH_SESSION, PN_LOCAL_ACTIVE, PN_LOCAL_CLOSED, PN_LOCAL_UNINIT, PN_NONDURABLE, PN_RCV_FIRST, \
    PN_RCV_SECOND, PN_REMOTE_ACTIVE, PN_REMOTE_CLOSED, PN_REMOTE_UNINIT, PN_SND_MIXED, PN_SND_SETTLED, PN_SND_UNSETTLED, \
    PN_SOURCE, PN_TARGET, PN_UNSPECIFIED, pn_connection, pn_connection_attachments, pn_connection_close, \
    pn_connection_collect, pn_connection_condition, pn_connection_desired_capabilities, pn_connection_error, \
    pn_connection_get_authorization, pn_connection_get_container, pn_connection_get_hostname, pn_connection_get_user, \
    pn_connection_offered_capabilities, \
    pn_connection_open, pn_connection_properties, pn_connection_release, pn_connection_remote_condition, \
    pn_connection_remote_container, pn_connection_remote_desired_capabilities, pn_connection_remote_hostname, \
    pn_connection_remote_offered_capabilities, pn_connection_remote_properties, \
    pn_connection_set_authorization, pn_connection_set_container, \
    pn_connection_set_hostname, pn_connection_set_password, pn_connection_set_user, pn_connection_state, \
    pn_connection_transport, pn_delivery, pn_error_code, pn_error_text, pn_link_advance, pn_link_attachments, \
    pn_link_available, pn_link_close, pn_link_condition, pn_link_credit, pn_link_current, pn_link_detach, pn_link_drain, \
    pn_link_drained, pn_link_draining, pn_link_error, pn_link_flow, pn_link_free, pn_link_get_drain, pn_link_head, \
    pn_link_is_receiver, pn_link_is_sender, pn_link_max_message_size, pn_link_name, pn_link_next, pn_link_offered, \
    pn_link_open, pn_link_queued, pn_link_rcv_settle_mode, pn_link_recv, pn_link_remote_condition, \
    pn_link_remote_max_message_size, pn_link_remote_rcv_settle_mode, pn_link_remote_snd_settle_mode, \
    pn_link_remote_source, pn_link_remote_target, pn_link_send, pn_link_session, pn_link_set_drain, \
    pn_link_set_max_message_size, pn_link_set_rcv_settle_mode, pn_link_set_snd_settle_mode, pn_link_snd_settle_mode, \
    pn_link_source, pn_link_state, pn_link_target, pn_link_unsettled, pn_receiver, pn_sender, pn_session, \
    pn_session_attachments, pn_session_close, pn_session_condition, pn_session_connection, pn_session_free, \
    pn_session_get_incoming_capacity, pn_session_get_outgoing_window, pn_session_head, pn_session_incoming_bytes, \
    pn_session_next, pn_session_open, pn_session_outgoing_bytes, pn_session_remote_condition, \
    pn_session_set_incoming_capacity, pn_session_set_outgoing_window, pn_session_state, pn_terminus_capabilities, \
    pn_terminus_copy, pn_terminus_filter, pn_terminus_get_address, pn_terminus_get_distribution_mode, \
    pn_terminus_get_durability, pn_terminus_get_expiry_policy, pn_terminus_get_timeout, pn_terminus_get_type, \
    pn_terminus_is_dynamic, pn_terminus_outcomes, pn_terminus_properties, pn_terminus_set_address, \
    pn_terminus_set_distribution_mode, pn_terminus_set_durability, pn_terminus_set_dynamic, \
    pn_terminus_set_expiry_policy, pn_terminus_set_timeout, pn_terminus_set_type, pn_work_head, \
    pn_link_properties, pn_link_remote_properties


def _optional(value):
    if value == ffi.NULL:
        return None
    return value


# TODO trololo, had pass here
def pn_record_get(record, pyctx):
    """Calling ffi.from_handle(p) is invalid and will likely crash if the cdata object returned by new_handle() is not kept alive!"""
    ret = lib.pn_record_get(record, pyctx)
    if ret == ffi.NULL:
        return None
    return ffi.from_handle(ret)  # todo start using py2void thing...


def pn_record_set(record, pyctx, payload):
    """todo skip return for void methods
    trololo, forgot to write this"""
    return lib.pn_record_set(record, pyctx, ffi.new_handle(payload))


def pn_connection_remote_container(connection):
    op = lib.pn_connection_remote_container(connection)
    # if op == ffi.cast("char *", ffi.NULL):
    if op == ffi.NULL:
        return None
    return ffi.string(op).decode()


def pn_connection_remote_hostname(connection):
    ret = lib.pn_connection_remote_hostname(connection)
    if ret == ffi.NULL:
        return None
    return ffi.string(ret).decode()


def pn_session_head(connection, state):
    ret = lib.pn_session_head(connection, state)
    if ret == ffi.NULL:
        return None
    return ret


def pn_condition_get_name(cond):
    ret = lib.pn_condition_get_name(cond)
    if ret == ffi.NULL:
        return None
    return ffi.string(ret).decode()


def pn_delivery(link, tag):
    b = tag.encode()
    cp = ffi.new('char[]', b)
    pn_bytes_t_tag = {'start': cp, 'size': len(b)}
    return lib.pn_delivery(link, pn_bytes_t_tag)


def pn_link_send(link, data):
    size = len(data)
    return lib.pn_link_send(link, data, size)


def pn_link_recv(link, limit: int):
    buffer = ffi.new('char[]', limit)
    cb = lib.pn_link_recv(link, buffer, limit)
    if cb < 0:
        return cb, None  # don't forget this!
    return cb, ffi.unpack(buffer, cb)  # careful about null bytes in things that are not strings


def pn_delivery_tag(delivery) -> str:
    """Todo when autogenerating these things based on c types, write pyi file or put python inline"""
    pn_bytes_t = lib.pn_delivery_tag(delivery)
    return ffi.unpack(pn_bytes_t.start, pn_bytes_t.size).decode()


def pn_link_next(link, mask):
    return _optional(lib.pn_link_next(link, mask))
