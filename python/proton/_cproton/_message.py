import uuid

from _proton_core import ffi
from _proton_core import lib
from _proton_core.lib import PN_DEFAULT_PRIORITY, PN_OVERFLOW, pn_error_text, pn_message, \
    pn_message_get_correlation_id, \
    pn_message_set_correlation_id, \
    pn_message_set_id, \
    pn_message_annotations, pn_message_body, pn_message_clear, pn_message_correlation_id, pn_message_decode, \
    pn_message_encode, pn_message_error, pn_message_free, pn_message_get_address, pn_message_get_content_encoding, \
    pn_message_get_content_type, pn_message_get_creation_time, pn_message_get_delivery_count, \
    pn_message_get_expiry_time, pn_message_get_group_id, pn_message_get_group_sequence, pn_message_get_priority, \
    pn_message_get_reply_to, pn_message_get_reply_to_group_id, pn_message_get_subject, pn_message_get_ttl, \
    pn_message_get_user_id, pn_message_get_id, pn_message_instructions, pn_message_is_durable, \
    pn_message_is_first_acquirer, \
    pn_message_is_inferred, pn_message_properties, pn_message_set_address, pn_message_set_content_encoding, \
    pn_message_set_content_type, pn_message_set_creation_time, pn_message_set_delivery_count, pn_message_set_durable, \
    pn_message_set_expiry_time, pn_message_set_first_acquirer, pn_message_set_group_id, pn_message_set_group_sequence, \
    pn_message_set_inferred, pn_message_set_priority, pn_message_set_reply_to, pn_message_set_reply_to_group_id, \
    pn_message_set_subject, pn_message_set_ttl, pn_message_set_user_id, \
    PN_NULL, PN_BOOL, PN_UBYTE, PN_BYTE, PN_USHORT, PN_SHORT, PN_UINT, PN_INT, PN_CHAR, PN_ULONG, PN_LONG, PN_TIMESTAMP, \
    PN_FLOAT, PN_DOUBLE, PN_DECIMAL32, PN_DECIMAL64, PN_DECIMAL128, PN_UUID, PN_BINARY, PN_STRING, PN_SYMBOL, \
    PN_DESCRIBED, PN_ARRAY, PN_LIST, PN_MAP, PN_INVALID


def _to_python_str(d):
    if d == ffi.NULL:
        return None
    return ffi.string(d).decode()


def from_pn_atom_t(value):
    assert value.type != PN_INVALID
    if value.type == PN_NULL:
        return None
    if value.type == PN_STRING:
        return ffi.string(value.u.as_bytes.start, value.u.as_bytes.size).decode()
    if value.type == PN_ULONG:
        return value.u.as_ulong
    if value.type == PN_BINARY:
        return ffi.string(value.u.as_bytes.start, value.u.as_bytes.size)
    if value.type == PN_UUID:
        return uuid.UUID(bytes=ffi.string(value.u.as_uuid.bytes))
    assert False


def to_pn_msgid_t(value):
    if value is None:
        return {'type': PN_NULL}

    if isinstance(value, bool):
        return {'type': PN_BOOL, 'u': {'as_bool': value}}

    if isinstance(value, int):
        assert 0 <= value  # todo maximum
        return {'type': PN_ULONG, 'u': {'as_ulong': value}}

    if isinstance(value, bytes):
        chars = ffi.new('char[]', value)
        return {'type': PN_BINARY, 'u': {'as_bytes': {'start': chars, 'size': len(value)}}}

    if isinstance(value, str):
        encoded = value.encode()
        chars = ffi.new('char[]', encoded)
        return {'type': PN_STRING, 'u': {'as_bytes': {'start': chars, 'size': len(encoded)}}}
        # todo special case for empty string?

    # uuid
    if isinstance(value, tuple):
        type_, bytes_ = value
        # data = ffi.new('pn_uuid_t', )
        return {'type': type_, 'u': {'as_uuid': {'bytes': bytes_}}}

    """
        case T_FLOAT:
        $1.type = PN_FLOAT;
        $1.u.as_float = NUM2DBL($input);
        break;

        case T_FIXNUM:
          $1.type = PN_INT;
          $1.u.as_int = FIX2LONG($input);
          break;

        case T_BIGNUM:
          $1.type = PN_LONG;
          $1.u.as_long = NUM2LL($input);
          break;

        }
    }
}
"""
    assert False, value


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
    bytes_ = ffi.new("char []", sz)
    status_code = lib.pn_message_encode(msg, bytes_, size)
    return status_code, ffi.buffer(bytes_, size[0])[:]


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


def pn_message_get_correlation_id(msg):
    return from_pn_atom_t(lib.pn_message_get_correlation_id(msg))


def pn_message_set_correlation_id(msg, value):
    atom = to_pn_msgid_t(value)
    return lib.pn_message_set_correlation_id(msg, atom)

def pn_message_get_id(msg):
    return from_pn_atom_t(lib.pn_message_get_id(msg))


def pn_message_set_id(msg, value):
    atom = to_pn_msgid_t(value)
    return lib.pn_message_set_id(msg, atom)
