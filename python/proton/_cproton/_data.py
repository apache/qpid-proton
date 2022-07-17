from _proton_core import ffi
from _proton_core import lib
from _proton_core.lib import PN_ARRAY, PN_BINARY, PN_BOOL, PN_BYTE, PN_CHAR, PN_DECIMAL128, PN_DECIMAL32, PN_DECIMAL64, \
    PN_DESCRIBED, PN_DOUBLE, PN_FLOAT, PN_INT, PN_LIST, PN_LONG, PN_MAP, PN_NULL, PN_OVERFLOW, PN_SHORT, PN_STRING, \
    PN_SYMBOL, PN_TIMESTAMP, PN_UBYTE, PN_UINT, PN_ULONG, PN_USHORT, PN_UUID, pn_data, pn_data_clear, pn_data_copy, \
    pn_data_decode, pn_data_dump, pn_data_encode, pn_data_encoded_size, pn_data_enter, pn_data_error, pn_data_exit, \
    pn_data_format, pn_data_free, pn_data_get_array, pn_data_get_array_type, pn_data_get_binary, pn_data_get_bool, \
    pn_data_get_byte, pn_data_get_char, pn_data_get_decimal128, pn_data_get_decimal32, pn_data_get_decimal64, \
    pn_data_get_double, pn_data_get_float, pn_data_get_int, pn_data_get_list, pn_data_get_long, pn_data_get_map, \
    pn_data_get_short, pn_data_get_string, pn_data_get_symbol, pn_data_get_timestamp, pn_data_get_ubyte, \
    pn_data_get_uint, pn_data_get_ulong, pn_data_get_ushort, pn_data_get_uuid, pn_data_is_array_described, \
    pn_data_is_described, pn_data_is_null, pn_data_lookup, pn_data_narrow, pn_data_next, pn_data_prev, \
    pn_data_put_array, pn_data_put_binary, pn_data_put_bool, pn_data_put_byte, pn_data_put_char, pn_data_put_decimal128, \
    pn_data_put_decimal32, pn_data_put_decimal64, pn_data_put_described, pn_data_put_double, pn_data_put_float, \
    pn_data_put_int, pn_data_put_list, pn_data_put_long, pn_data_put_map, pn_data_put_null, pn_data_put_short, \
    pn_data_put_string, pn_data_put_symbol, pn_data_put_timestamp, pn_data_put_ubyte, pn_data_put_uint, \
    pn_data_put_ulong, pn_data_put_ushort, pn_data_put_uuid, pn_data_rewind, pn_data_type, pn_data_widen, pn_error_text, \
    pn_bytes, pn_data_errno


def pn_data_lookup(data, name):
    return lib.pn_data_lookup(data, name.encode())


def pn_data_encode(data, sz):
    """"""
    dst = ffi.new('char[]', sz)
    encoded_size = lib.pn_data_encode(data, dst, sz)
    if encoded_size >= 0:
        dst = ffi.buffer(dst, encoded_size)
    return encoded_size, dst


def pn_data_decode(buffer, data):
    return lib.pn_data_decode(buffer, ffi.from_buffer(data), len(data))


def pn_data_put_binary(data, b):
    return lib.pn_data_put_binary(data, pn_bytes(len(b), b))


def pn_data_put_string(data, s):
    pnbytes = pn_bytes(len(s), s)
    return lib.pn_data_put_string(data, pnbytes)


def pn_data_put_symbol(data, s):
    pnbytes = lib.pn_bytes(len(s), s)
    symbol = lib.pn_data_put_symbol(data, pnbytes)
    return symbol


def pn_data_get_decimal128(data):
    res = lib.pn_data_get_decimal128(data)
    # copy data before res goes out of scope; the lifetime-related behavior here is quite unexpected
    return ffi.buffer(res.bytes)[:]


def pn_data_get_uuid(data):
    res = lib.pn_data_get_uuid(data)
    return bytes(ffi.buffer(res.bytes))


def pn_data_get_binary(data):
    pn_bytes = lib.pn_data_get_binary(data)
    binary = ffi.buffer(pn_bytes.start, pn_bytes.size)[:]
    return binary


def pn_data_get_string(data):
    pn_bytes = lib.pn_data_get_string(data)
    decode = ffi.string(pn_bytes.start, pn_bytes.size)
    return decode


def pn_data_format(data, size):
    buffer = ffi.new('char []', size)
    sz = ffi.new('size_t *', size)
    err = lib.pn_data_format(data, buffer, sz)
    result = ffi.buffer(buffer, sz[0])
    return err, result


def pn_data_get_symbol(data):
    pnbytes = lib.pn_data_get_symbol(data)
    symbol = ffi.string(pnbytes.start, pnbytes.size)
    return symbol


def pn_data_put_uuid(data, u):
    pn_uuid = ffi.new('pn_uuid_t *')
    pn_uuid.bytes = u
    uuid = lib.pn_data_put_uuid(data, pn_uuid[0])
    return uuid


def pn_data_put_decimal128(data, d):
    pn_decimal128 = ffi.new('pn_decimal128_t *')
    pn_decimal128.bytes = d
    decimal_ = lib.pn_data_put_decimal128(data, pn_decimal128[0])
    return decimal_


def pn_error_text(data):
    return ffi.string(lib.pn_error_text(data)).decode('utf8')
