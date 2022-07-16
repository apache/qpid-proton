from typing import Optional

from _proton_core import ffi, lib

from _proton_core.lib import PN_EOS, PN_OK, PN_SASL_AUTH, PN_SASL_NONE, PN_SASL_OK, PN_SASL_PERM, PN_SASL_SYS, \
    PN_SASL_TEMP, \
    PN_SSL_ANONYMOUS_PEER, PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY, PN_SSL_CERT_SUBJECT_COMMON_NAME, \
    PN_SSL_CERT_SUBJECT_COUNTRY_NAME, PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME, PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT, \
    PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE, PN_SSL_MD5, PN_SSL_MODE_CLIENT, PN_SSL_MODE_SERVER, PN_SSL_RESUME_NEW, \
    PN_SSL_RESUME_REUSED, PN_SSL_RESUME_UNKNOWN, PN_SSL_SHA1, PN_SSL_SHA256, PN_SSL_SHA512, PN_SSL_VERIFY_PEER, \
    PN_SSL_VERIFY_PEER_NAME, PN_TRACE_DRV, PN_TRACE_FRM, PN_TRACE_OFF, PN_TRACE_RAW, pn_error_text, pn_sasl, \
    pn_sasl_allowed_mechs, pn_sasl_config_name, pn_sasl_config_path, pn_sasl_done, pn_sasl_extended, \
    pn_sasl_get_allow_insecure_mechs, pn_sasl_get_mech, pn_sasl_get_user, pn_sasl_get_authorization, pn_sasl_outcome, \
    pn_sasl_set_allow_insecure_mechs, pn_ssl, pn_ssl_domain, pn_ssl_domain_allow_unsecured_client, pn_ssl_domain_free, \
    pn_ssl_domain_set_credentials, pn_ssl_domain_set_peer_authentication, pn_ssl_domain_set_trusted_ca_db, \
    pn_ssl_get_cert_fingerprint, pn_ssl_get_cipher_name, pn_ssl_get_peer_hostname, pn_ssl_get_protocol_name, \
    pn_ssl_get_remote_subject, pn_ssl_get_remote_subject_subfield, pn_ssl_init, pn_ssl_present, pn_ssl_resume_status, \
    pn_ssl_set_peer_hostname, pn_transport, pn_transport_attachments, pn_transport_bind, pn_transport_capacity, \
    pn_transport_close_head, pn_transport_close_tail, pn_transport_closed, pn_transport_condition, \
    pn_transport_connection, pn_transport_error, pn_transport_get_channel_max, pn_transport_get_frames_input, \
    pn_transport_get_frames_output, pn_transport_get_idle_timeout, pn_transport_get_max_frame, \
    pn_transport_get_pytracer, pn_transport_get_remote_idle_timeout, pn_transport_get_remote_max_frame, \
    pn_transport_get_user, pn_transport_is_authenticated, pn_transport_is_encrypted, pn_transport_log, \
    pn_transport_peek, pn_transport_pending, pn_transport_pop, pn_transport_push, pn_transport_remote_channel_max, \
    pn_transport_require_auth, pn_transport_require_encryption, pn_transport_set_channel_max, \
    pn_transport_set_idle_timeout, pn_transport_set_max_frame, pn_transport_set_pytracer, pn_transport_set_server, \
    pn_transport_tick, pn_transport_trace, pn_transport_unbind


def _str_to_charp(value):
    if value is None:
        return ffi.NULL
    return value.encode()  # necessary, initializer for ctype 'char *' must be a bytes or list or tuple, not str


def pn_transport_peek(transport, size):
    dst_t = ffi.new("char []", size)
    cd = lib.pn_transport_peek(transport, dst_t, size)
    if cd < 0:
        return cd, None
    return cd, ffi.unpack(dst_t, cd)


def pn_transport_push(transport, binary):
    # TODO OMG, such copypaste mistake!
    src = ffi.new("char[]", binary)
    return lib.pn_transport_push(transport, src, len(binary))


def pn_ssl_domain_set_credentials(domain, cert, key, password):
    if password is None:
        return lib.pn_ssl_domain_set_credentials(domain, cert.encode(), key.encode(), ffi.NULL)

    return lib.pn_ssl_domain_set_credentials(domain, cert.encode(), key.encode(), password.encode())


def pn_ssl_domain_set_trusted_ca_db(domain, certificate_db):
    return lib.pn_ssl_domain_set_trusted_ca_db(domain, certificate_db.encode())


def pn_ssl_domain_set_peer_authentication(domain, verify_mode, trusted_CA):
    tca = trusted_CA
    if tca is not None:
        tca = tca.encode()
    else:
        tca = ffi.NULL
    return lib.pn_ssl_domain_set_peer_authentication(domain, verify_mode, tca)


def _optional_string(s) -> Optional[str]:
    """todo pretty much everything that returns char * should get this"""

    if s == ffi.NULL:
        return None
    return ffi.string(s).decode()


def pn_transport_get_user(transport):
    return _optional_string(lib.pn_transport_get_user(transport))


def pn_sasl_get_user(sasl):
    return _optional_string(lib.pn_sasl_get_user(sasl))


def pn_sasl_get_authorization(sasl):
    return _optional_string(lib.pn_sasl_get_authorization(sasl))


def pn_ssl_init(ssl, domain, session_id):
    return lib.pn_ssl_init(ssl, domain, _str_to_charp(session_id))


def pn_sasl_get_mech(sasl):
    return _optional_string(lib.pn_sasl_get_mech(sasl))


# todo: another pattern, the buffer and the size
def pn_ssl_get_cipher_name(ssl, size):
    buffer = ffi.new('char[]', size)
    rc = lib.pn_ssl_get_cipher_name(ssl, buffer, size)
    name = ffi.string(buffer, size)
    return rc, name


def pn_ssl_get_protocol_name(ssl, size):
    buffer = ffi.new('char[]', size)
    rc = lib.pn_ssl_get_protocol_name(ssl, buffer, size)
    name = ffi.string(buffer, size)
    return rc, name


def pn_ssl_get_peer_hostname(ssl, size):
    """when size is a pointer, it is necessary to do it this way"""
    size_t = ffi.new('size_t *', size)
    buffer = ffi.new('char[]', size)
    rc = lib.pn_ssl_get_peer_hostname(ssl, buffer, size_t)
    name = ffi.string(buffer, size_t[0])
    return rc, name


def pn_ssl_get_cert_fingerprint(ssl, fingerprint_length, digest_name):
    fingerprint_buffer = ffi.new('char[]', fingerprint_length)
    rc = lib.pn_ssl_get_cert_fingerprint(ssl, fingerprint_buffer, fingerprint_length, digest_name)
    fingerprint = ffi.string(fingerprint_buffer, fingerprint_length).decode()
    return rc, fingerprint


def pn_ssl_get_remote_subject_subfield(ssl, subfield_name):
    return _optional_string(lib.pn_ssl_get_remote_subject_subfield(ssl, subfield_name))


def pn_ssl_get_remote_subject(ssl):
    return _optional_string(lib.pn_ssl_get_remote_subject(ssl))


def _optional(param):
    if param == ffi.NULL:
        return None
    return param


def pn_connection_attachments(connection):
    return _optional(lib.pn_connection_attachments(connection))


def pn_transport_log(transport, message):
    return lib.pn_transport_log(transport, _str_to_charp(message))
