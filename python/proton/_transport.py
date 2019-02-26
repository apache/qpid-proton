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

from __future__ import absolute_import

from cproton import PN_SASL_AUTH, PN_SASL_PERM, PN_SASL_SYS, PN_SSL_RESUME_REUSED, PN_SASL_NONE, PN_SSL_SHA1, \
    PN_SSL_CERT_SUBJECT_COUNTRY_NAME, PN_SASL_OK, PN_SSL_RESUME_UNKNOWN, PN_EOS, PN_SSL_ANONYMOUS_PEER, PN_SSL_MD5, \
    PN_SSL_CERT_SUBJECT_COMMON_NAME, PN_SSL_VERIFY_PEER, PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY, PN_SSL_MODE_SERVER, \
    PN_TRACE_DRV, PN_TRACE_RAW, pn_transport, PN_SSL_SHA256, PN_TRACE_FRM, PN_SSL_MODE_CLIENT, PN_SASL_TEMP, \
    PN_SSL_SHA512, PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT, PN_OK, PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE, \
    PN_SSL_VERIFY_PEER_NAME, PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME, PN_SSL_RESUME_NEW, PN_TRACE_OFF, \
    pn_transport_get_channel_max, pn_transport_capacity, pn_transport_push, pn_transport_get_user, pn_transport_tick, \
    pn_transport_set_max_frame, pn_transport_attachments, pn_transport_unbind, pn_transport_peek, \
    pn_transport_set_channel_max, pn_transport_close_tail, pn_transport_condition, pn_transport_is_encrypted, \
    pn_transport_get_frames_input, pn_transport_bind, pn_transport_closed, pn_transport_get_idle_timeout, \
    pn_transport_get_remote_idle_timeout, pn_transport_get_frames_output, pn_transport_pending, \
    pn_transport_set_pytracer, pn_transport_close_head, pn_transport_get_remote_max_frame, \
    pn_transport_is_authenticated, pn_transport_set_idle_timeout, pn_transport_log, pn_transport_get_pytracer, \
    pn_transport_require_auth, pn_transport_get_max_frame, pn_transport_set_server, pn_transport_remote_channel_max, \
    pn_transport_require_encryption, pn_transport_pop, pn_transport_connection, \
    pn_sasl, pn_sasl_set_allow_insecure_mechs, pn_sasl_outcome, pn_transport_error, pn_sasl_get_user, \
    pn_sasl_extended, pn_sasl_done, pn_sasl_get_allow_insecure_mechs, pn_sasl_allowed_mechs, \
    pn_sasl_config_name, pn_sasl_config_path, \
    pn_ssl, pn_ssl_init, pn_ssl_domain_allow_unsecured_client, pn_ssl_domain_free, \
    pn_ssl_domain, pn_transport_trace, pn_ssl_resume_status, pn_sasl_get_mech, \
    pn_ssl_domain_set_trusted_ca_db, pn_ssl_get_remote_subject_subfield, pn_ssl_present, \
    pn_ssl_get_remote_subject, pn_ssl_domain_set_credentials, pn_ssl_domain_set_peer_authentication, \
    pn_ssl_get_peer_hostname, pn_ssl_set_peer_hostname, pn_ssl_get_cipher_name, pn_ssl_get_cert_fingerprint, \
    pn_ssl_get_protocol_name, \
    pn_error_text

from ._common import millis2secs, secs2millis, unicode2utf8, utf82unicode
from ._condition import cond2obj
from ._exceptions import EXCEPTIONS, TransportException, SessionException, SSLException, SSLUnavailable
from ._wrapper import Wrapper


class TraceAdapter:

    def __init__(self, tracer):
        self.tracer = tracer

    def __call__(self, trans_impl, message):
        self.tracer(Transport.wrap(trans_impl), message)


class Transport(Wrapper):
    TRACE_OFF = PN_TRACE_OFF
    TRACE_DRV = PN_TRACE_DRV
    TRACE_FRM = PN_TRACE_FRM
    TRACE_RAW = PN_TRACE_RAW

    CLIENT = 1
    SERVER = 2

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None
        else:
            return Transport(_impl=impl)

    def __init__(self, mode=None, _impl=pn_transport):
        Wrapper.__init__(self, _impl, pn_transport_attachments)
        if mode == Transport.SERVER:
            pn_transport_set_server(self._impl)
        elif mode is None or mode == Transport.CLIENT:
            pass
        else:
            raise TransportException("Cannot initialise Transport from mode: %s" % str(mode))

    def _init(self):
        self._sasl = None
        self._ssl = None

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, TransportException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_transport_error(self._impl))))
        else:
            return err

    def _set_tracer(self, tracer):
        pn_transport_set_pytracer(self._impl, TraceAdapter(tracer))

    def _get_tracer(self):
        adapter = pn_transport_get_pytracer(self._impl)
        if adapter:
            return adapter.tracer
        else:
            return None

    tracer = property(_get_tracer, _set_tracer,
                      doc="""
A callback for trace logging. The callback is passed the transport and log message.
""")

    def log(self, message):
        pn_transport_log(self._impl, message)

    def require_auth(self, bool):
        pn_transport_require_auth(self._impl, bool)

    @property
    def authenticated(self):
        return pn_transport_is_authenticated(self._impl)

    def require_encryption(self, bool):
        pn_transport_require_encryption(self._impl, bool)

    @property
    def encrypted(self):
        return pn_transport_is_encrypted(self._impl)

    @property
    def user(self):
        return pn_transport_get_user(self._impl)

    def bind(self, connection):
        """Assign a connection to the transport"""
        self._check(pn_transport_bind(self._impl, connection._impl))

    def unbind(self):
        """Release the connection"""
        self._check(pn_transport_unbind(self._impl))

    def trace(self, n):
        pn_transport_trace(self._impl, n)

    def tick(self, now):
        """Process any timed events (like heartbeat generation).
        now = seconds since epoch (float).
        """
        return millis2secs(pn_transport_tick(self._impl, secs2millis(now)))

    def capacity(self):
        c = pn_transport_capacity(self._impl)
        if c >= PN_EOS:
            return c
        else:
            return self._check(c)

    def push(self, binary):
        n = self._check(pn_transport_push(self._impl, binary))
        if n != len(binary):
            raise OverflowError("unable to process all bytes: %s, %s" % (n, len(binary)))

    def close_tail(self):
        self._check(pn_transport_close_tail(self._impl))

    def pending(self):
        p = pn_transport_pending(self._impl)
        if p >= PN_EOS:
            return p
        else:
            return self._check(p)

    def peek(self, size):
        cd, out = pn_transport_peek(self._impl, size)
        if cd == PN_EOS:
            return None
        else:
            self._check(cd)
            return out

    def pop(self, size):
        pn_transport_pop(self._impl, size)

    def close_head(self):
        self._check(pn_transport_close_head(self._impl))

    @property
    def closed(self):
        return pn_transport_closed(self._impl)

    # AMQP 1.0 max-frame-size
    def _get_max_frame_size(self):
        return pn_transport_get_max_frame(self._impl)

    def _set_max_frame_size(self, value):
        pn_transport_set_max_frame(self._impl, value)

    max_frame_size = property(_get_max_frame_size, _set_max_frame_size,
                              doc="""
Sets the maximum size for received frames (in bytes).
""")

    @property
    def remote_max_frame_size(self):
        return pn_transport_get_remote_max_frame(self._impl)

    def _get_channel_max(self):
        return pn_transport_get_channel_max(self._impl)

    def _set_channel_max(self, value):
        if pn_transport_set_channel_max(self._impl, value):
            raise SessionException("Too late to change channel max.")

    channel_max = property(_get_channel_max, _set_channel_max,
                           doc="""
Sets the maximum channel that may be used on the transport.
""")

    @property
    def remote_channel_max(self):
        return pn_transport_remote_channel_max(self._impl)

    # AMQP 1.0 idle-time-out
    def _get_idle_timeout(self):
        return millis2secs(pn_transport_get_idle_timeout(self._impl))

    def _set_idle_timeout(self, sec):
        pn_transport_set_idle_timeout(self._impl, secs2millis(sec))

    idle_timeout = property(_get_idle_timeout, _set_idle_timeout,
                            doc="""
The idle timeout of the connection (float, in seconds).
""")

    @property
    def remote_idle_timeout(self):
        return millis2secs(pn_transport_get_remote_idle_timeout(self._impl))

    @property
    def frames_output(self):
        return pn_transport_get_frames_output(self._impl)

    @property
    def frames_input(self):
        return pn_transport_get_frames_input(self._impl)

    def sasl(self):
        return SASL(self)

    def ssl(self, domain=None, session_details=None):
        # SSL factory (singleton for this transport)
        if not self._ssl:
            self._ssl = SSL(self, domain, session_details)
        return self._ssl

    @property
    def condition(self):
        return cond2obj(pn_transport_condition(self._impl))

    @property
    def connection(self):
        from . import _endpoints
        return _endpoints.Connection.wrap(pn_transport_connection(self._impl))


class SASLException(TransportException):
    pass


class SASL(Wrapper):
    OK = PN_SASL_OK
    AUTH = PN_SASL_AUTH
    SYS = PN_SASL_SYS
    PERM = PN_SASL_PERM
    TEMP = PN_SASL_TEMP

    @staticmethod
    def extended():
        return pn_sasl_extended()

    def __init__(self, transport):
        Wrapper.__init__(self, transport._impl, pn_transport_attachments)
        self._sasl = pn_sasl(transport._impl)

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, SASLException)
            raise exc("[%s]" % (err))
        else:
            return err

    @property
    def user(self):
        return pn_sasl_get_user(self._sasl)

    @property
    def mech(self):
        return pn_sasl_get_mech(self._sasl)

    @property
    def outcome(self):
        outcome = pn_sasl_outcome(self._sasl)
        if outcome == PN_SASL_NONE:
            return None
        else:
            return outcome

    def allowed_mechs(self, mechs):
        if isinstance(mechs, list):
            mechs = " ".join(mechs)
        pn_sasl_allowed_mechs(self._sasl, unicode2utf8(mechs))

    def _get_allow_insecure_mechs(self):
        return pn_sasl_get_allow_insecure_mechs(self._sasl)

    def _set_allow_insecure_mechs(self, insecure):
        pn_sasl_set_allow_insecure_mechs(self._sasl, insecure)

    allow_insecure_mechs = property(_get_allow_insecure_mechs, _set_allow_insecure_mechs,
                                    doc="""
Allow unencrypted cleartext passwords (PLAIN mech)
""")

    def done(self, outcome):
        pn_sasl_done(self._sasl, outcome)

    def config_name(self, name):
        pn_sasl_config_name(self._sasl, name)

    def config_path(self, path):
        pn_sasl_config_path(self._sasl, path)


class SSLDomain(object):
    MODE_CLIENT = PN_SSL_MODE_CLIENT
    MODE_SERVER = PN_SSL_MODE_SERVER
    VERIFY_PEER = PN_SSL_VERIFY_PEER
    VERIFY_PEER_NAME = PN_SSL_VERIFY_PEER_NAME
    ANONYMOUS_PEER = PN_SSL_ANONYMOUS_PEER

    def __init__(self, mode):
        self._domain = pn_ssl_domain(mode)
        if self._domain is None:
            raise SSLUnavailable()

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, SSLException)
            raise exc("SSL failure.")
        else:
            return err

    def set_credentials(self, cert_file, key_file, password):
        return self._check(pn_ssl_domain_set_credentials(self._domain,
                                                         cert_file, key_file,
                                                         password))

    def set_trusted_ca_db(self, certificate_db):
        return self._check(pn_ssl_domain_set_trusted_ca_db(self._domain,
                                                           certificate_db))

    def set_peer_authentication(self, verify_mode, trusted_CAs=None):
        return self._check(pn_ssl_domain_set_peer_authentication(self._domain,
                                                                 verify_mode,
                                                                 trusted_CAs))

    def allow_unsecured_client(self):
        return self._check(pn_ssl_domain_allow_unsecured_client(self._domain))

    def __del__(self):
        pn_ssl_domain_free(self._domain)


class SSL(object):

    @staticmethod
    def present():
        return pn_ssl_present()

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, SSLException)
            raise exc("SSL failure.")
        else:
            return err

    def __new__(cls, transport, domain, session_details=None):
        """Enforce a singleton SSL object per Transport"""
        if transport._ssl:
            # unfortunately, we've combined the allocation and the configuration in a
            # single step.  So catch any attempt by the application to provide what
            # may be a different configuration than the original (hack)
            ssl = transport._ssl
            if (domain and (ssl._domain is not domain) or
                    session_details and (ssl._session_details is not session_details)):
                raise SSLException("Cannot re-configure existing SSL object!")
        else:
            obj = super(SSL, cls).__new__(cls)
            obj._domain = domain
            obj._session_details = session_details
            session_id = None
            if session_details:
                session_id = session_details.get_session_id()
            obj._ssl = pn_ssl(transport._impl)
            if obj._ssl is None:
                raise SSLUnavailable()
            if domain:
                pn_ssl_init(obj._ssl, domain._domain, session_id)
            transport._ssl = obj
        return transport._ssl

    def cipher_name(self):
        rc, name = pn_ssl_get_cipher_name(self._ssl, 128)
        if rc:
            return name
        return None

    def protocol_name(self):
        rc, name = pn_ssl_get_protocol_name(self._ssl, 128)
        if rc:
            return name
        return None

    SHA1 = PN_SSL_SHA1
    SHA256 = PN_SSL_SHA256
    SHA512 = PN_SSL_SHA512
    MD5 = PN_SSL_MD5

    CERT_COUNTRY_NAME = PN_SSL_CERT_SUBJECT_COUNTRY_NAME
    CERT_STATE_OR_PROVINCE = PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE
    CERT_CITY_OR_LOCALITY = PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY
    CERT_ORGANIZATION_NAME = PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME
    CERT_ORGANIZATION_UNIT = PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT
    CERT_COMMON_NAME = PN_SSL_CERT_SUBJECT_COMMON_NAME

    def get_cert_subject_subfield(self, subfield_name):
        subfield_value = pn_ssl_get_remote_subject_subfield(self._ssl, subfield_name)
        return subfield_value

    def get_cert_subject(self):
        subject = pn_ssl_get_remote_subject(self._ssl)
        return subject

    def _get_cert_subject_unknown_subfield(self):
        # Pass in an unhandled enum
        return self.get_cert_subject_subfield(10)

    # Convenience functions for obtaining the subfields of the subject field.
    def get_cert_common_name(self):
        return self.get_cert_subject_subfield(SSL.CERT_COMMON_NAME)

    def get_cert_organization(self):
        return self.get_cert_subject_subfield(SSL.CERT_ORGANIZATION_NAME)

    def get_cert_organization_unit(self):
        return self.get_cert_subject_subfield(SSL.CERT_ORGANIZATION_UNIT)

    def get_cert_locality_or_city(self):
        return self.get_cert_subject_subfield(SSL.CERT_CITY_OR_LOCALITY)

    def get_cert_country(self):
        return self.get_cert_subject_subfield(SSL.CERT_COUNTRY_NAME)

    def get_cert_state_or_province(self):
        return self.get_cert_subject_subfield(SSL.CERT_STATE_OR_PROVINCE)

    def get_cert_fingerprint(self, fingerprint_length, digest_name):
        rc, fingerprint_str = pn_ssl_get_cert_fingerprint(self._ssl, fingerprint_length, digest_name)
        if rc == PN_OK:
            return fingerprint_str
        return None

    # Convenience functions for obtaining fingerprint for specific hashing algorithms
    def _get_cert_fingerprint_unknown_hash_alg(self):
        return self.get_cert_fingerprint(41, 10)

    def get_cert_fingerprint_sha1(self):
        return self.get_cert_fingerprint(41, SSL.SHA1)

    def get_cert_fingerprint_sha256(self):
        # sha256 produces a fingerprint that is 64 characters long
        return self.get_cert_fingerprint(65, SSL.SHA256)

    def get_cert_fingerprint_sha512(self):
        # sha512 produces a fingerprint that is 128 characters long
        return self.get_cert_fingerprint(129, SSL.SHA512)

    def get_cert_fingerprint_md5(self):
        return self.get_cert_fingerprint(33, SSL.MD5)

    @property
    def remote_subject(self):
        return pn_ssl_get_remote_subject(self._ssl)

    RESUME_UNKNOWN = PN_SSL_RESUME_UNKNOWN
    RESUME_NEW = PN_SSL_RESUME_NEW
    RESUME_REUSED = PN_SSL_RESUME_REUSED

    def resume_status(self):
        return pn_ssl_resume_status(self._ssl)

    def _set_peer_hostname(self, hostname):
        self._check(pn_ssl_set_peer_hostname(self._ssl, unicode2utf8(hostname)))

    def _get_peer_hostname(self):
        err, name = pn_ssl_get_peer_hostname(self._ssl, 1024)
        self._check(err)
        return utf82unicode(name)

    peer_hostname = property(_get_peer_hostname, _set_peer_hostname,
                             doc="""
Manage the expected name of the remote peer.  Used to authenticate the remote.
""")


class SSLSessionDetails(object):
    """ Unique identifier for the SSL session.  Used to resume previous session on a new
    SSL connection.
    """

    def __init__(self, session_id):
        self._session_id = session_id

    def get_session_id(self):
        return self._session_id
