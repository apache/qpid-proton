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

import atexit
from uuid import UUID

from cproton_ffi import ffi, lib
from cproton_ffi.lib import (PN_ACCEPTED, PN_ARRAY, PN_BINARY, PN_BOOL, PN_BYTE, PN_CHAR,
                             PN_CONFIGURATION, PN_CONNECTION_BOUND, PN_CONNECTION_FINAL,
                             PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_CLOSE,
                             PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_REMOTE_CLOSE,
                             PN_CONNECTION_REMOTE_OPEN, PN_CONNECTION_UNBOUND, PN_COORDINATOR,
                             PN_DECIMAL32, PN_DECIMAL64, PN_DECIMAL128, PN_DEFAULT_PRIORITY,
                             PN_DELIVERIES, PN_DELIVERY, PN_DESCRIBED, PN_DIST_MODE_COPY,
                             PN_DIST_MODE_MOVE, PN_DIST_MODE_UNSPECIFIED, PN_DOUBLE, PN_EOS,
                             PN_EVENT_NONE, PN_EXPIRE_NEVER, PN_EXPIRE_WITH_CONNECTION,
                             PN_EXPIRE_WITH_LINK, PN_EXPIRE_WITH_SESSION, PN_FLOAT, PN_INT, PN_INTR,
                             PN_LINK_FINAL, PN_LINK_FLOW, PN_LINK_INIT, PN_LINK_LOCAL_CLOSE,
                             PN_LINK_LOCAL_DETACH, PN_LINK_LOCAL_OPEN, PN_LINK_REMOTE_CLOSE,
                             PN_LINK_REMOTE_DETACH, PN_LINK_REMOTE_OPEN, PN_LIST, PN_LOCAL_ACTIVE,
                             PN_LOCAL_CLOSED, PN_LOCAL_UNINIT, PN_LONG, PN_MAP, PN_MODIFIED,
                             PN_NONDURABLE, PN_NULL, PN_OK, PN_OVERFLOW, PN_RCV_FIRST,
                             PN_RCV_SECOND, PN_RECEIVED, PN_REJECTED, PN_RELEASED, PN_REMOTE_ACTIVE,
                             PN_REMOTE_CLOSED, PN_REMOTE_UNINIT, PN_SASL_AUTH, PN_SASL_NONE,
                             PN_SASL_OK, PN_SASL_PERM, PN_SASL_SYS, PN_SASL_TEMP, PN_SESSION_FINAL,
                             PN_SESSION_INIT, PN_SESSION_LOCAL_CLOSE, PN_SESSION_LOCAL_OPEN,
                             PN_SESSION_REMOTE_CLOSE, PN_SESSION_REMOTE_OPEN, PN_SHORT,
                             PN_SND_MIXED, PN_SND_SETTLED, PN_SND_UNSETTLED, PN_SOURCE,
                             PN_SSL_ANONYMOUS_PEER, PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY,
                             PN_SSL_CERT_SUBJECT_COMMON_NAME, PN_SSL_CERT_SUBJECT_COUNTRY_NAME,
                             PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME,
                             PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT,
                             PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE, PN_SSL_MD5, PN_SSL_MODE_CLIENT,
                             PN_SSL_MODE_SERVER, PN_SSL_RESUME_NEW, PN_SSL_RESUME_REUSED,
                             PN_SSL_RESUME_UNKNOWN, PN_SSL_SHA1, PN_SSL_SHA256, PN_SSL_SHA512,
                             PN_SSL_VERIFY_PEER, PN_SSL_VERIFY_PEER_NAME, PN_STRING, PN_SYMBOL,
                             PN_TARGET, PN_TIMEOUT, PN_TIMER_TASK, PN_TIMESTAMP, PN_TRACE_DRV,
                             PN_TRACE_FRM, PN_TRACE_OFF, PN_TRACE_RAW, PN_TRANSPORT,
                             PN_TRANSPORT_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED,
                             PN_TRANSPORT_TAIL_CLOSED, PN_UBYTE, PN_UINT, PN_ULONG, PN_UNSPECIFIED,
                             PN_USHORT, PN_UUID, PN_VERSION_MAJOR, PN_VERSION_MINOR,
                             PN_VERSION_POINT, pn_cast_pn_connection, pn_cast_pn_delivery,
                             pn_cast_pn_link, pn_cast_pn_session, pn_cast_pn_transport,
                             pn_collector, pn_collector_free, pn_collector_more, pn_collector_peek,
                             pn_collector_pop, pn_collector_release, pn_condition_clear,
                             pn_condition_info, pn_condition_is_set, pn_connection,
                             pn_connection_attachments, pn_connection_close, pn_connection_collect,
                             pn_connection_condition, pn_connection_desired_capabilities,
                             pn_connection_error, pn_connection_offered_capabilities,
                             pn_connection_open, pn_connection_properties, pn_connection_release,
                             pn_connection_remote_condition,
                             pn_connection_remote_desired_capabilities,
                             pn_connection_remote_offered_capabilities,
                             pn_connection_remote_properties, pn_connection_state,
                             pn_connection_transport, pn_data, pn_data_clear, pn_data_copy,
                             pn_data_dump, pn_data_encoded_size, pn_data_enter, pn_data_error,
                             pn_data_exit, pn_data_free, pn_data_get_array, pn_data_get_array_type,
                             pn_data_get_bool, pn_data_get_byte, pn_data_get_char,
                             pn_data_get_decimal32, pn_data_get_decimal64, pn_data_get_double,
                             pn_data_get_float, pn_data_get_int, pn_data_get_list, pn_data_get_long,
                             pn_data_get_map, pn_data_get_short, pn_data_get_timestamp,
                             pn_data_get_ubyte, pn_data_get_uint, pn_data_get_ulong,
                             pn_data_get_ushort, pn_data_is_array_described, pn_data_is_described,
                             pn_data_is_null, pn_data_narrow, pn_data_next, pn_data_prev,
                             pn_data_put_array, pn_data_put_bool, pn_data_put_byte,
                             pn_data_put_char, pn_data_put_decimal32, pn_data_put_decimal64,
                             pn_data_put_described, pn_data_put_double, pn_data_put_float,
                             pn_data_put_int, pn_data_put_list, pn_data_put_long, pn_data_put_map,
                             pn_data_put_null, pn_data_put_short, pn_data_put_timestamp,
                             pn_data_put_ubyte, pn_data_put_uint, pn_data_put_ulong,
                             pn_data_put_ushort, pn_data_rewind, pn_data_type, pn_data_widen,
                             pn_decref, pn_delivery_abort, pn_delivery_aborted,
                             pn_delivery_attachments, pn_delivery_link, pn_delivery_local,
                             pn_delivery_local_state, pn_delivery_partial, pn_delivery_pending,
                             pn_delivery_readable, pn_delivery_remote, pn_delivery_remote_state,
                             pn_delivery_settle, pn_delivery_settled, pn_delivery_update,
                             pn_delivery_updated, pn_delivery_writable, pn_disposition_annotations,
                             pn_disposition_condition, pn_disposition_data,
                             pn_disposition_get_section_number, pn_disposition_get_section_offset,
                             pn_disposition_is_failed, pn_disposition_is_undeliverable,
                             pn_disposition_set_failed, pn_disposition_set_section_number,
                             pn_disposition_set_section_offset, pn_disposition_set_undeliverable,
                             pn_disposition_type, pn_error_code, pn_event_connection,
                             pn_event_context, pn_event_delivery, pn_event_link, pn_event_session,
                             pn_event_transport, pn_event_type, pn_incref, pn_link_advance,
                             pn_link_attachments, pn_link_available, pn_link_close,
                             pn_link_condition, pn_link_credit, pn_link_current, pn_link_detach,
                             pn_link_drain, pn_link_drained, pn_link_draining, pn_link_error,
                             pn_link_flow, pn_link_free, pn_link_get_drain, pn_link_head,
                             pn_link_is_receiver, pn_link_is_sender, pn_link_max_message_size,
                             pn_link_next, pn_link_offered, pn_link_open, pn_link_properties,
                             pn_link_queued, pn_link_rcv_settle_mode, pn_link_remote_condition,
                             pn_link_remote_max_message_size, pn_link_remote_properties,
                             pn_link_remote_rcv_settle_mode, pn_link_remote_snd_settle_mode,
                             pn_link_remote_source, pn_link_remote_target, pn_link_session,
                             pn_link_set_drain, pn_link_set_max_message_size,
                             pn_link_set_rcv_settle_mode, pn_link_set_snd_settle_mode,
                             pn_link_snd_settle_mode, pn_link_source, pn_link_state, pn_link_target,
                             pn_link_unsettled, pn_message, pn_message_annotations, pn_message_body,
                             pn_message_clear, pn_message_error, pn_message_free,
                             pn_message_get_creation_time, pn_message_get_delivery_count,
                             pn_message_get_expiry_time, pn_message_get_group_sequence,
                             pn_message_get_priority, pn_message_get_ttl, pn_message_instructions,
                             pn_message_is_durable, pn_message_is_first_acquirer,
                             pn_message_is_inferred, pn_message_properties,
                             pn_message_set_creation_time, pn_message_set_delivery_count,
                             pn_message_set_durable, pn_message_set_expiry_time,
                             pn_message_set_first_acquirer, pn_message_set_group_sequence,
                             pn_message_set_inferred, pn_message_set_priority, pn_message_set_ttl,
                             pn_sasl, pn_sasl_done, pn_sasl_extended,
                             pn_sasl_get_allow_insecure_mechs, pn_sasl_outcome,
                             pn_sasl_set_allow_insecure_mechs, pn_session, pn_session_attachments,
                             pn_session_close, pn_session_condition, pn_session_connection,
                             pn_session_free, pn_session_get_incoming_capacity,
                             pn_session_get_outgoing_window, pn_session_head,
                             pn_session_incoming_bytes, pn_session_next, pn_session_open,
                             pn_session_outgoing_bytes, pn_session_remote_condition,
                             pn_session_set_incoming_capacity, pn_session_set_outgoing_window,
                             pn_session_state, pn_ssl, pn_ssl_domain,
                             pn_ssl_domain_allow_unsecured_client, pn_ssl_domain_free,
                             pn_ssl_present, pn_ssl_resume_status, pn_terminus_capabilities,
                             pn_terminus_copy, pn_terminus_filter,
                             pn_terminus_get_distribution_mode, pn_terminus_get_durability,
                             pn_terminus_get_expiry_policy, pn_terminus_get_timeout,
                             pn_terminus_get_type, pn_terminus_is_dynamic, pn_terminus_outcomes,
                             pn_terminus_properties, pn_terminus_set_distribution_mode,
                             pn_terminus_set_durability, pn_terminus_set_dynamic,
                             pn_terminus_set_expiry_policy, pn_terminus_set_timeout,
                             pn_terminus_set_type, pn_transport, pn_transport_attachments,
                             pn_transport_bind, pn_transport_capacity, pn_transport_close_head,
                             pn_transport_close_tail, pn_transport_closed, pn_transport_condition,
                             pn_transport_connection, pn_transport_error,
                             pn_transport_get_channel_max, pn_transport_get_frames_input,
                             pn_transport_get_frames_output, pn_transport_get_idle_timeout,
                             pn_transport_get_max_frame, pn_transport_get_remote_idle_timeout,
                             pn_transport_get_remote_max_frame, pn_transport_is_authenticated,
                             pn_transport_is_encrypted, pn_transport_pending, pn_transport_pop,
                             pn_transport_remote_channel_max, pn_transport_require_auth,
                             pn_transport_require_encryption, pn_transport_set_channel_max,
                             pn_transport_set_idle_timeout, pn_transport_set_max_frame,
                             pn_transport_set_server, pn_transport_tick, pn_transport_trace,
                             pn_transport_unbind)


def isnull(obj):
    return obj is None or obj == ffi.NULL


def addressof(obj):
    return int(ffi.cast('uint64_t', obj))


def void2py(void):
    if void == ffi.NULL:
        return None
    return ffi.from_handle(void)


def string2utf8(string):
    """Convert python string into bytes compatible with char* C string"""
    if string is None:
        return ffi.NULL
    elif isinstance(string, str):
        return string.encode('utf8')
    # Anything else illegal - specifically python3 bytes
    raise TypeError("Unrecognized string type: %r (%s)" % (string, type(string)))


def utf82string(string):
    """Convert char* C strings returned from proton-c into python unicode"""
    if string == ffi.NULL:
        return None
    return ffi.string(string).decode('utf8')


def bytes2py(b):
    return memoryview(ffi.buffer(b.start, b.size))


def bytes2pybytes(b):
    return bytes(ffi.buffer(b.start, b.size))


def bytes2string(b, encoding='utf8'):
    return ffi.unpack(b.start, b.size).decode(encoding)


def py2bytes(py):
    if isinstance(py, (bytes, bytearray,memoryview)):
        s = ffi.from_buffer(py)
        return len(s), s
    elif isinstance(py, str):
        s = ffi.from_buffer(py.encode('utf8'))
        return len(s), s


def string2bytes(py, encoding='utf8'):
    s = ffi.from_buffer(py.encode(encoding))
    return len(s), s


def UUID2uuid(py):
    u = ffi.new('pn_uuid_t*')
    ffi.memmove(u.bytes, py.bytes, 16)
    return u[0]


def uuid2bytes(uuid):
    return ffi.unpack(uuid.bytes, 16)


def decimal1282py(decimal128):
    return ffi.unpack(decimal128.bytes, 16)


def py2decimal128(py):
    d = ffi.new('pn_decimal128_t*')
    ffi.memmove(d.bytes, py, 16)
    return d[0]


def msgid2py(msgid):
    t = msgid.type
    if t == PN_NULL:
        return None
    elif t == PN_ULONG:
        return msgid.u.as_ulong
    elif t == PN_BINARY:
        return bytes2py(msgid.u.as_bytes)
    elif t == PN_STRING:
        return bytes2string(msgid.u.as_bytes)
    elif t == PN_UUID:
        return UUID(bytes=uuid2bytes(msgid.u.as_uuid))
    # These two cases are for compatibility with the broken ruby binding
    elif t == PN_INT:
        v = msgid.u.as_int
        if v >= 0:
            return v
        return None
    elif t == PN_LONG:
        v = msgid.u.as_long
        if v >= 0:
            return v
        return None
    return None


def py2msgid(py):
    if py is None:
        return {'type': PN_NULL}
    elif isinstance(py, int):
        return {'type': PN_ULONG, 'u': {'as_ulong': py}}
    elif isinstance(py, str):
        return {'type': PN_STRING, 'u': {'as_bytes': string2bytes(py)}}
    elif isinstance(py, bytes):
        return {'type': PN_BINARY, 'u': {'as_bytes': py2bytes(py)}}
    elif isinstance(py, UUID):
        return {'type': PN_UUID, 'u': {'as_uuid': {'bytes': py.bytes}}}
    elif isinstance(py, tuple):
        if py[0] == PN_UUID:
            return {'type': PN_UUID, 'u': {'as_uuid': {'bytes': py[1]}}}
    return {'type': PN_NULL}


@ffi.def_extern()
def pn_pytracer(transport, message):
    attrs = pn_record_get_py(lib.pn_transport_attachments(transport))
    tracer = attrs['_tracer']
    if tracer:
        tracer(transport, utf82string(message))


def pn_transport_get_pytracer(transport):
    attrs = pn_record_get_py(lib.pn_transport_attachments(transport))
    if '_tracer' in attrs:
        return attrs['_tracer']
    else:
        return None


def pn_transport_set_pytracer(transport, tracer):
    attrs = pn_record_get_py(lib.pn_transport_attachments(transport))
    attrs['_tracer'] = tracer
    lib.pn_transport_set_tracer(transport, lib.pn_pytracer)

retained_objects = set()
lib.init()

@atexit.register
def clear_retained_objects():
    retained_objects.clear()

def retained_count():
    """ Debugging aid to give the number of wrapper objects retained by the bindings"""
    return len(retained_objects)

@ffi.def_extern()
def pn_pyref_incref(obj):
    retained_objects.add(obj)


@ffi.def_extern()
def pn_pyref_decref(obj):
    retained_objects.discard(obj)


def pn_tostring(obj):
    cs = lib.pn_tostring(obj)
    s = ffi.string(cs).decode('utf8')
    lib.free(cs)
    return s


def pn_collector_put_pyref(collector, obj, etype):
    d = ffi.new_handle(obj)
    retained_objects.add(d)
    lib.pn_collector_put_py(collector, d, etype.number)


def pn_record_def_py(record):
    lib.pn_record_def_py(record)


def pn_record_get_py(record):
    d = lib.pn_record_get_py(record)
    if d == ffi.NULL:
        return None
    return ffi.from_handle(d)


def pn_record_set_py(record, value):
    if value is None:
        d = ffi.NULL
    else:
        d = ffi.new_handle(value)
        retained_objects.add(d)
    lib.pn_record_set_py(record, d)


def pn_event_class_name(event):
    return ffi.string(lib.pn_event_class_name_py(event)).decode('utf8')


# size_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
def pn_transport_peek(transport, size):
    buff = bytearray(size)
    cd = lib.pn_transport_peek(transport, ffi.from_buffer(buff), size)
    if cd >= 0:
        buff = buff[:cd]
    return cd, buff


# ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
def pn_transport_push(transport, src):
    return lib.pn_transport_push(transport, ffi.from_buffer(src), len(src))


# int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
def pn_message_decode(msg, buff):
    return lib.pn_message_decode(msg, ffi.from_buffer(buff), len(buff))


# int pn_message_encode_py(pn_message_t *msg, char *bytes, size_t size);
def pn_message_encode(msg, size):
    buff = bytearray(size)
    err = lib.pn_message_encode_py(msg, ffi.from_buffer(buff), size)
    if err >= 0:
        buff = buff[:err]
    return err, buff


# ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);
def pn_data_decode(data, buff):
    return lib.pn_data_decode(data, ffi.from_buffer(buff), len(buff))


# ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
def pn_data_encode(data, size):
    buff = bytearray(size)
    err = lib.pn_data_encode(data, ffi.from_buffer(buff), size)
    if err >= 0:
        buff = buff[:err]
    return err, buff


# int pn_data_format(pn_data_t *data, char *bytes, size_t *size);
def pn_data_format(data, size):
    buff = bytearray(size)
    err = lib.pn_data_format_py(data, ffi.from_buffer(buff), size)
    if err >= 0:
        buff = buff[:err]
    return err, buff


# ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
def pn_link_recv(receiver, limit):
    buff = bytearray(limit)
    err = lib.pn_link_recv(receiver, ffi.from_buffer(buff), limit)
    if err >= 0:
        buff = buff[:err]
    return err, buff


# ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
def pn_link_send(sender, buff):
    return lib.pn_link_send(sender, ffi.from_buffer(buff), len(buff))


# pn_condition bindings
def pn_condition_set_name(cond, name):
    return lib.pn_condition_set_name(cond, string2utf8(name))


def pn_condition_set_description(cond, description):
    return lib.pn_condition_set_description(cond, string2utf8(description))


def pn_condition_get_name(cond):
    return utf82string(lib.pn_condition_get_name(cond))


def pn_condition_get_description(cond):
    return utf82string(lib.pn_condition_get_description(cond))


# pn_error bindings
def pn_error_text(error):
    return utf82string(lib.pn_error_text(error))

# pn_data bindings
def pn_data_lookup(data, name):
    return lib.pn_data_lookup(data, string2utf8(name))

def pn_data_put_decimal128(data, d):
    return lib.pn_data_put_decimal128(data, py2decimal128(d))


def pn_data_put_uuid(data, u):
    return lib.pn_data_put_uuid(data, UUID2uuid(u))


def pn_data_put_binary(data, b):
    return lib.pn_data_put_binary(data, py2bytes(b))


def pn_data_put_string(data, s):
    return lib.pn_data_put_string(data, string2bytes(s))


def pn_data_put_symbol(data, s):
    return lib.pn_data_put_symbol(data, string2bytes(s, 'ascii'))

def pn_data_get_decimal128(data):
    return decimal1282py(lib.pn_data_get_decimal128(data))


def pn_data_get_uuid(data):
    return UUID(bytes=uuid2bytes(lib.pn_data_get_uuid(data)))


def pn_data_get_binary(data):
    return bytes2py(lib.pn_data_get_binary(data))


def pn_data_get_string(data):
    return bytes2string(lib.pn_data_get_string(data))


def pn_data_get_symbol(data):
    return bytes2string(lib.pn_data_get_symbol(data), 'ascii')


def pn_delivery_tag(delivery):
    return bytes2string(lib.pn_delivery_tag(delivery))


def pn_connection_get_container(connection):
    return utf82string(lib.pn_connection_get_container(connection))


def pn_connection_set_container(connection, name):
    lib.pn_connection_set_container(connection, string2utf8(name))


def pn_connection_get_hostname(connection):
    return utf82string(lib.pn_connection_get_hostname(connection))


def pn_connection_set_hostname(connection, name):
    lib.pn_connection_set_hostname(connection, string2utf8(name))


def pn_connection_get_user(connection):
    return utf82string(lib.pn_connection_get_user(connection))


def pn_connection_set_user(connection, name):
    lib.pn_connection_set_user(connection, string2utf8(name))


def pn_connection_get_authorization(connection):
    return utf82string(lib.pn_connection_get_authorization(connection))


def pn_connection_set_authorization(connection, name):
    lib.pn_connection_set_authorization(connection, string2utf8(name))


def pn_connection_set_password(connection, name):
    lib.pn_connection_set_password(connection, string2utf8(name))


def pn_connection_remote_container(connection):
    return utf82string(lib.pn_connection_remote_container(connection))


def pn_connection_remote_hostname(connection):
    return utf82string(lib.pn_connection_remote_hostname(connection))


def pn_sender(session, name):
    return lib.pn_sender(session, string2utf8(name))


def pn_receiver(session, name):
    return lib.pn_receiver(session, string2utf8(name))


def pn_delivery(link, tag):
    return lib.pn_delivery(link, py2bytes(tag))

def pn_link_name(link):
    return utf82string(lib.pn_link_name(link))


def pn_terminus_get_address(terminus):
    return utf82string(lib.pn_terminus_get_address(terminus))


def pn_terminus_set_address(terminus, address):
    return lib.pn_terminus_set_address(terminus, string2utf8(address))


def pn_event_type_name(number):
    return utf82string(lib.pn_event_type_name(number))


def pn_message_get_id(message):
    return msgid2py(lib.pn_message_get_id(message))


def pn_message_set_id(message, value):
    lib.pn_message_set_id(message, py2msgid(value))


def pn_message_get_user_id(message):
    return bytes2pybytes(lib.pn_message_get_user_id(message))


def pn_message_set_user_id(message, value):
    return lib.pn_message_set_user_id(message, py2bytes(value))


def pn_message_get_address(message):
    return utf82string(lib.pn_message_get_address(message))


def pn_message_set_address(message, value):
    return lib.pn_message_set_address(message, string2utf8(value))


def pn_message_get_subject(message):
    return utf82string(lib.pn_message_get_subject(message))


def pn_message_set_subject(message, value):
    return lib.pn_message_set_subject(message, string2utf8(value))


def pn_message_get_reply_to(message):
    return utf82string(lib.pn_message_get_reply_to(message))


def pn_message_set_reply_to(message, value):
    return lib.pn_message_set_reply_to(message, string2utf8(value))


def pn_message_get_correlation_id(message):
    return msgid2py(lib.pn_message_get_correlation_id(message))


def pn_message_set_correlation_id(message, value):
    lib.pn_message_set_correlation_id(message, py2msgid(value))


def pn_message_get_content_type(message):
    return utf82string(lib.pn_message_get_content_type(message))


def pn_message_set_content_type(message, value):
    return lib.pn_message_set_content_type(message, string2utf8(value))


def pn_message_get_content_encoding(message):
    return utf82string(lib.pn_message_get_content_encoding(message))


def pn_message_set_content_encoding(message, value):
    return lib.pn_message_set_content_encoding(message, string2utf8(value))


def pn_message_get_group_id(message):
    return utf82string(lib.pn_message_get_group_id(message))


def pn_message_set_group_id(message, value):
    return lib.pn_message_set_group_id(message, string2utf8(value))


def pn_message_get_reply_to_group_id(message):
    return utf82string(lib.pn_message_get_reply_to_group_id(message))


def pn_message_set_reply_to_group_id(message, value):
    return lib.pn_message_set_reply_to_group_id(message, string2utf8(value))


def pn_transport_log(transport, message):
    lib.pn_transport_log(transport, string2utf8(message))

def pn_transport_get_user(transport):
    return utf82string(lib.pn_transport_get_user(transport))


def pn_sasl_get_user(sasl):
    return utf82string(lib.pn_sasl_get_user(sasl))


def pn_sasl_get_authorization(sasl):
    return utf82string(lib.pn_sasl_get_authorization(sasl))


def pn_sasl_get_mech(sasl):
    return utf82string(lib.pn_sasl_get_mech(sasl))


def pn_sasl_allowed_mechs(sasl, mechs):
    lib.pn_sasl_allowed_mechs(sasl, string2utf8(mechs))


def pn_sasl_config_name(sasl, name):
    lib.pn_sasl_config_name(sasl, string2utf8(name))


def pn_sasl_config_path(sasl, path):
    lib.pn_sasl_config_path(sasl, string2utf8(path))

def pn_ssl_domain_set_credentials(domain, cert_file, key_file, password):
    return lib.pn_ssl_domain_set_credentials(domain, string2utf8(cert_file), string2utf8(key_file), string2utf8(password))


def pn_ssl_domain_set_trusted_ca_db(domain, certificate_db):
    return lib.pn_ssl_domain_set_trusted_ca_db(domain, string2utf8(certificate_db))


def pn_ssl_domain_set_peer_authentication(domain, verify_mode, trusted_CAs):
    return lib.pn_ssl_domain_set_peer_authentication(domain, verify_mode, string2utf8(trusted_CAs))


def pn_ssl_init(ssl, domain, session_id):
    lib.pn_ssl_init(ssl, domain, string2utf8(session_id))


def pn_ssl_get_remote_subject_subfield(ssl, subfield_name):
    return utf82string(lib.pn_ssl_get_remote_subject_subfield(ssl, subfield_name))


def pn_ssl_get_remote_subject(ssl):
    return  utf82string(lib.pn_ssl_get_remote_subject(ssl))


# int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols);
def pn_ssl_domain_set_protocols(domain, protocols):
    return lib.pn_ssl_domain_set_protocols(domain, string2utf8(protocols))


# int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers);
def pn_ssl_domain_set_ciphers(domain, ciphers):
    return lib.pn_ssl_domain_set_ciphers(domain, string2utf8(ciphers))


# _Bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);
def pn_ssl_get_cipher_name(ssl, size):
    buff = ffi.new('char[]', size)
    r = lib.pn_ssl_get_cipher_name(ssl, buff, size)
    if r:
        return utf82string(buff)
    return None


# _Bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
def pn_ssl_get_protocol_name(ssl, size):
    buff = ffi.new('char[]', size)
    r = lib.pn_ssl_get_protocol_name(ssl, buff, size)
    if r:
        return utf82string(buff)
    return None


# int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl, char *fingerprint, size_t fingerprint_len, pn_ssl_hash_alg hash_alg);
def pn_ssl_get_cert_fingerprint(ssl, fingerprint_len, hash_alg):
    buff = ffi.new('char[]', fingerprint_len)
    r = lib.pn_ssl_get_cert_fingerprint(ssl, buff, fingerprint_len, hash_alg)
    if r==PN_OK:
        return utf82string(buff)
    return None


# int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *hostname, size_t *bufsize);
def pn_ssl_get_peer_hostname(ssl, size):
    buff = ffi.new('char[]', size)
    r = lib.pn_ssl_get_peer_hostname_py(ssl, buff, size)
    if r==PN_OK:
        return r, utf82string(buff)
    return r, None

def pn_ssl_set_peer_hostname(ssl, hostname):
    return lib.pn_ssl_set_peer_hostname(ssl, string2utf8(hostname))

