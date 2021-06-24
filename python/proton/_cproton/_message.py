from _proton_core import ffi
from _proton_core import lib
from _proton_core.lib import PN_DEFAULT_PRIORITY, PN_OVERFLOW, pn_error_text, pn_message, \
    pn_message_annotations, pn_message_body, pn_message_clear, pn_message_correlation_id, pn_message_decode, \
    pn_message_encode, pn_message_error, pn_message_free, pn_message_get_address, pn_message_get_content_encoding, \
    pn_message_get_content_type, pn_message_get_creation_time, pn_message_get_delivery_count, \
    pn_message_get_expiry_time, pn_message_get_group_id, pn_message_get_group_sequence, pn_message_get_priority, \
    pn_message_get_reply_to, pn_message_get_reply_to_group_id, pn_message_get_subject, pn_message_get_ttl, \
    pn_message_get_user_id, pn_message_id, pn_message_instructions, pn_message_is_durable, pn_message_is_first_acquirer, \
    pn_message_is_inferred, pn_message_properties, pn_message_set_address, pn_message_set_content_encoding, \
    pn_message_set_content_type, pn_message_set_creation_time, pn_message_set_delivery_count, pn_message_set_durable, \
    pn_message_set_expiry_time, pn_message_set_first_acquirer, pn_message_set_group_id, pn_message_set_group_sequence, \
    pn_message_set_inferred, pn_message_set_priority, pn_message_set_reply_to, pn_message_set_reply_to_group_id, \
    pn_message_set_subject, pn_message_set_ttl, pn_message_set_user_id


def _to_python_str(d):
    if d == ffi.NULL:
        return None
    return ffi.string(d).decode()


def pn_message_get_user_id(msg):
    r = lib.pn_message_get_user_id(msg)
    if r.start == ffi.NULL:
        return b""
    return ffi.buffer(r.start, r.size)[:]


def pn_message_set_user_id(msg, value):
    pnbytes = lib.pn_bytes(len(value), value)
    err = lib.pn_message_set_user_id(msg, pnbytes)
    return err


def pn_message_set_content_encoding(msg, value):
    return lib.pn_message_set_content_encoding(msg, value.encode())


def pn_message_set_reply_to(msg, value):
    return lib.pn_message_set_reply_to(msg, value.encode())


def pn_message_set_subject(msg, value):
    return lib.pn_message_set_subject(msg, value.encode())


def pn_message_set_address(msg, value):
    return lib.pn_message_set_address(msg, value.encode())


def pn_message_set_reply_to_group_id(msg, value):
    return lib.pn_message_set_reply_to_group_id(msg, value.encode())


def pn_message_set_group_id(msg, value):
    return lib.pn_message_set_group_id(msg, value.encode())


def pn_message_set_content_type(msg, value):
    return lib.pn_message_set_content_type(msg, value.encode())


def pn_message_encode(msg, sz):
    size = ffi.new('size_t *', sz)
    bytes = ffi.new("char []", sz)
    status_code = lib.pn_message_encode(msg, bytes, size)
    return status_code, ffi.buffer(bytes, size[0])[:]


def pn_message_decode(msg, data):
    bytes = ffi.new('char []', data)
    size = ffi.new("size_t *", len(data))
    return lib.pn_message_decode(msg, bytes, size[0])


def pn_message_get_address(msg):
    return _to_python_str(lib.pn_message_get_address(msg))


def pn_message_get_subject(msg):
    return _to_python_str(lib.pn_message_get_subject(msg))


def pn_message_get_reply_to_group_id(msg):
    return _to_python_str(lib.pn_message_get_reply_to_group_id(msg))


def pn_message_get_reply_to(msg):
    return _to_python_str(lib.pn_message_get_reply_to(msg))


def pn_message_get_group_id(msg):
    return _to_python_str(lib.pn_message_get_group_id(msg))


def pn_message_get_content_type(msg):
    return _to_python_str(lib.pn_message_get_content_type(msg))


def pn_message_get_content_encoding(msg):
    return _to_python_str(lib.pn_message_get_content_encoding(msg))
