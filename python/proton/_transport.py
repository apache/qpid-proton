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

from typing import Callable, Optional, Type, Union, TYPE_CHECKING, List

from cproton import PN_EOS, PN_OK, PN_SASL_AUTH, PN_SASL_NONE, PN_SASL_OK, PN_SASL_PERM, PN_SASL_SYS, PN_SASL_TEMP, \
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
    pn_transport_tick, pn_transport_trace, pn_transport_unbind, \
    isnull

from ._common import millis2secs, secs2millis
from ._condition import cond2obj, obj2cond
from ._exceptions import EXCEPTIONS, SSLException, SSLUnavailable, SessionException, TransportException
from ._wrapper import Wrapper

if TYPE_CHECKING:
    from ._condition import Condition
    from ._endpoints import Connection  # would produce circular import


class TraceAdapter:

    def __init__(self, tracer: Callable[['Transport', str], None]) -> None:
        self.tracer = tracer

    def __call__(self, trans_impl, message):
        self.tracer(Transport.wrap(trans_impl), message)


class Transport(Wrapper):
    """
    A network channel supporting an AMQP connection.
    """

    TRACE_OFF = PN_TRACE_OFF
    """ Turn logging off entirely. """

    TRACE_DRV = PN_TRACE_DRV
    """  Log driver-related events. """

    TRACE_FRM = PN_TRACE_FRM
    """ Log protocol frames going in and out of the transport. """

    TRACE_RAW = PN_TRACE_RAW
    """ Log raw binary data going in and out of the transport. """

    CLIENT = 1
    """ Transport mode is as a client. """

    SERVER = 2
    """ Transport mode is as a server. """

    @staticmethod
    def wrap(impl: Optional[Callable]) -> Optional['Transport']:
        if isnull(impl):
            return None
        else:
            return Transport(impl=impl)

    def __init__(
            self,
            mode: 'Optional[int]' = None,
            impl: 'Callable' = None,
    ) -> None:
        if impl is None:
            Wrapper.__init__(self, constructor=pn_transport, get_context=pn_transport_attachments)
        else:
            Wrapper.__init__(self, impl, pn_transport_attachments)
        if mode == Transport.SERVER:
            pn_transport_set_server(self._impl)
        elif mode is None or mode == Transport.CLIENT:
            pass
        else:
            raise TransportException("Cannot initialise Transport from mode: %s" % str(mode))

    def _init(self) -> None:
        self._sasl = None
        self._ssl = None
        self._reactor = None
        self._connect_selectable = None

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, TransportException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_transport_error(self._impl))))
        else:
            return err

    @property
    def tracer(self) -> Optional[Callable[['Transport', str], None]]:
        """A callback for trace logging. The callback is passed the transport
        and log message. For no tracer callback, value is ``None``.
        """
        adapter = pn_transport_get_pytracer(self._impl)
        if adapter:
            return adapter.tracer
        else:
            return None

    @tracer.setter
    def tracer(self, tracer: Callable[['Transport', str], None]) -> None:
        pn_transport_set_pytracer(self._impl, TraceAdapter(tracer))

    def log(self, message: str) -> None:
        """
        Log a message using a transport's logging mechanism.

        This can be useful in a debugging context as the log message will
        be prefixed with the transport's identifier.

        :param message: The message to be logged.
        :type message: ``str``
        """
        pn_transport_log(self._impl, message)

    def require_auth(self, bool: bool) -> None:
        """
        Set whether a non-authenticated transport connection is allowed.

        There are several ways within the AMQP protocol suite to get
        unauthenticated connections:

            - Use no SASL layer (with either no TLS or TLS without client certificates)
            - Use a SASL layer but the ANONYMOUS mechanism

        The default if this option is not set is to allow unauthenticated connections.

        :param bool: ``True`` when authenticated connections are required.
        """
        pn_transport_require_auth(self._impl, bool)

    @property
    def authenticated(self) -> bool:
        """
        Indicate whether the transport connection is authenticated.

        .. note:: This property may not be stable until the :const:`Event.CONNECTION_REMOTE_OPEN`
            event is received.

        :type: ``bool``
        """
        return pn_transport_is_authenticated(self._impl)

    def require_encryption(self, bool):
        """
        Set whether a non encrypted transport connection is allowed

        There are several ways within the AMQP protocol suite to get encrypted connections:

            - Use TLS
            - Use a SASL with a mechanism that supports security layers

        The default if this option is not set is to allow unencrypted connections.

        :param bool: ``True`` if encryption is required on this transport, ``False`` otherwise.
        :type bool: ``bool``
        """
        pn_transport_require_encryption(self._impl, bool)

    @property
    def encrypted(self) -> bool:
        """
        Indicate whether the transport connection is encrypted.

        .. note:: This property may not be stable until the :const:`Event.CONNECTION_REMOTE_OPEN`
            event is received.
        """
        return pn_transport_is_encrypted(self._impl)

    @property
    def user(self) -> Optional[str]:
        """
        The authenticated user.

        On the client it will return whatever user was passed in to the
        :attr:`Connection.user` attribute of the bound connection.

        The returned value is only reliable after the ``PN_TRANSPORT_AUTHENTICATED``
        event has been received.
        """
        return pn_transport_get_user(self._impl)

    def bind(self, connection: 'Connection') -> None:
        """
        Assign a connection to the transport.

        :param connection: Connection to which to bind.
        :raise: :exc:`TransportException` if there is any Proton error.
        """
        self._check(pn_transport_bind(self._impl, connection._impl))

    def bind_nothrow(self, connection: 'Connection') -> None:
        """
        Assign a connection to the transport. Any failure is
        ignored rather than thrown.

        :param connection: Connection to which to bind.
        """
        pn_transport_bind(self._impl, connection._impl)

    def unbind(self) -> None:
        """
        Unbinds a transport from its AMQP connection.

        :raise: :exc:`TransportException` if there is any Proton error.
        """
        self._check(pn_transport_unbind(self._impl))

    def trace(self, n: int) -> None:
        """
        Update a transports trace flags.

        The trace flags for a transport control what sort of information is
        logged. The value may be :const:`TRACE_OFF` or any combination of
        :const:`TRACE_DRV`, :const:`TRACE_FRM`, :const:`TRACE_RAW` using
        a bitwise or operation.

        :param n: Trace flags
        """
        pn_transport_trace(self._impl, n)

    def tick(self, now: float) -> float:
        """
        Process any pending transport timer events (like heartbeat generation).

        This method should be called after all pending input has been
        processed by the transport and before generating output. It returns
        the deadline for the next pending timer event, if any are present.

        .. note:: This function does nothing until the first data is read
            from or written to the transport.

        :param now: seconds since epoch.
        :return: If non-zero, then the monotonic expiration time of the next
                 pending timer event for the transport.  The caller must invoke
                 :meth:`tick` again at least once at or before this deadline
                 occurs. If ``0.0``, then there are no pending events.
        """
        return millis2secs(pn_transport_tick(self._impl, secs2millis(now)))

    def capacity(self) -> int:
        """
        Get the amount of free space for input following the transport's
        tail pointer.

        :return: Available space for input in bytes.
        :raise: :exc:`TransportException` if there is any Proton error.
        """
        c = pn_transport_capacity(self._impl)
        if c >= PN_EOS:
            return c
        else:
            return self._check(c)

    def push(self, binary: bytes) -> None:
        """
        Pushes the supplied bytes into the tail of the transport.
        Only some of the bytes will be copied if there is insufficient
        capacity available. Use :meth:`capacity` to determine how much
        capacity the transport has.

        :param binary: Data to be pushed onto the transport tail.
        :raise: - :exc:`TransportException` if there is any Proton error.
                - ``OverflowError`` if the size of the data exceeds the
                  transport capacity.
        """
        n = self._check(pn_transport_push(self._impl, binary))
        if n != len(binary):
            raise OverflowError("unable to process all bytes: %s, %s" % (n, len(binary)))

    def close_tail(self) -> None:
        """
        Indicate that the input has reached End Of Stream (EOS).

        This tells the transport that no more input will be forthcoming.

        :raise: :exc:`TransportException` if there is any Proton error.
        """
        self._check(pn_transport_close_tail(self._impl))

    def pending(self) -> int:
        """
        Get the number of pending output bytes following the transport's
        head pointer.

        :return: The number of pending output bytes.
        :raise: :exc:`TransportException` if there is any Proton error.
        """
        p = pn_transport_pending(self._impl)
        if p >= PN_EOS:
            return p
        else:
            return self._check(p)

    def peek(self, size: int) -> Optional[bytes]:
        """
        Returns ``size`` bytes from the head of the transport.

        It is an error to call this with a value of ``size`` that
        is greater than the value reported by :meth:`pending`.

        :param size: Number of bytes to return.
        :return: ``size`` bytes from the head of the transport, or ``None``
                 if none are available.
        :raise: :exc:`TransportException` if there is any Proton error.
        """
        cd, out = pn_transport_peek(self._impl, size)
        if cd == PN_EOS:
            return None
        else:
            self._check(cd)
            return out

    def pop(self, size: int) -> None:
        """
        Removes ``size`` bytes of output from the pending output queue
        following the transport's head pointer.

        Calls to this function may alter the transport's head pointer as
        well as the number of pending bytes reported by
        :meth:`pending`.

        :param size: Number of bytes to remove.
        """
        pn_transport_pop(self._impl, size)

    def close_head(self) -> None:
        """
        Indicate that the output has closed.

        This tells the transport that no more output will be popped.

        :raise: :exc:`TransportException` if there is any Proton error.
        """
        self._check(pn_transport_close_head(self._impl))

    @property
    def closed(self) -> bool:
        """
        ``True`` iff both the transport head and transport tail are closed
        using :meth:`close_head` and :meth:`close_tail` respectively.
        """
        return pn_transport_closed(self._impl)

    # AMQP 1.0 max-frame-size
    @property
    def max_frame_size(self) -> int:
        """The maximum size for transport frames (in bytes)."""
        return pn_transport_get_max_frame(self._impl)

    @max_frame_size.setter
    def max_frame_size(self, value: int) -> None:
        pn_transport_set_max_frame(self._impl, value)

    @property
    def remote_max_frame_size(self) -> int:
        """
        The maximum frame size of a transport's remote peer (in bytes).
        """
        return pn_transport_get_remote_max_frame(self._impl)

    @property
    def channel_max(self) -> int:
        """The maximum channel number that may be used on this transport.

        .. note:: This is the maximum channel number allowed, giving a
            valid channel number range of ``[0 .. channel_max]``. Therefore the
            maximum number of simultaneously active channels will be
            channel_max plus 1.

        You can set this more than once to raise and lower
        the limit your application imposes on max channels for this
        transport.  However, smaller limits may be imposed by Proton,
        or by the remote peer.

        After the ``OPEN`` frame has been sent to the remote peer,
        further calls to this function will have no effect.

        :raise: :exc:`SessionException` if the ``OPEN`` frame has already
                been sent.
        """
        return pn_transport_get_channel_max(self._impl)

    @channel_max.setter
    def channel_max(self, value: int) -> None:
        if pn_transport_set_channel_max(self._impl, value):
            raise SessionException("Too late to change channel max.")

    @property
    def remote_channel_max(self) -> int:
        """
        The maximum allowed channel number of a transport's remote peer.
        """
        return pn_transport_remote_channel_max(self._impl)

    # AMQP 1.0 idle-time-out
    @property
    def idle_timeout(self) -> float:
        """The idle timeout of the connection in seconds. A zero idle
        timeout means heartbeats are disabled.
        """
        return millis2secs(pn_transport_get_idle_timeout(self._impl))

    @idle_timeout.setter
    def idle_timeout(self, sec: Union[float, int]) -> None:
        pn_transport_set_idle_timeout(self._impl, secs2millis(sec))

    @property
    def remote_idle_timeout(self) -> float:
        """
        Get the idle timeout for a transport's remote peer in
        seconds. A zero idle timeout means heartbeats are disabled.
        """
        return millis2secs(pn_transport_get_remote_idle_timeout(self._impl))

    @property
    def frames_output(self) -> int:
        """
        Get the number of frames output by a transport.
        """
        return pn_transport_get_frames_output(self._impl)

    @property
    def frames_input(self) -> int:
        """
        Get the number of frames input by a transport.
        """
        return pn_transport_get_frames_input(self._impl)

    def sasl(self) -> 'SASL':
        """
        Get the :class:`SASL` object associated with this transport.

        :return: SASL object associated with this transport.
        """
        return SASL(self)

    def ssl(self, domain: Optional['SSLDomain'] = None, session_details: Optional['SSLSessionDetails'] = None) -> 'SSL':
        """
        Get the :class:`SSL` session associated with this transport. If
        not set, then a new session will be created using ``domain`` and
        ``session_details``.

        :param domain: An SSL domain configuration object
        :param session_details: A unique identifier for the SSL session.
        :return: SSL session associated with this transport.
        """
        # SSL factory (singleton for this transport)
        if not self._ssl:
            self._ssl = SSL(self, domain, session_details)
        return self._ssl

    @property
    def condition(self) -> Optional['Condition']:
        """Get additional information about the condition of the transport.

        When a :const:`Event.TRANSPORT_ERROR` event occurs, this operation
        can be used to access the details of the error condition.

        See :class:`Condition` for more information.
        """
        return cond2obj(pn_transport_condition(self._impl))

    @condition.setter
    def condition(self, cond: 'Condition') -> None:
        pn_cond = pn_transport_condition(self._impl)
        obj2cond(cond, pn_cond)

    @property
    def connection(self) -> 'Connection':
        """The connection bound to this transport."""
        from . import _endpoints
        return _endpoints.Connection.wrap(pn_transport_connection(self._impl))


class SASLException(TransportException):
    pass


class SASL(Wrapper):
    """
    The SASL layer is responsible for establishing an authenticated
    and/or encrypted tunnel over which AMQP frames are passed between
    peers. The peer acting as the SASL Client must provide
    authentication credentials. The peer acting as the SASL Server must
    provide authentication against the received credentials.
    """
    OK = PN_SASL_OK
    AUTH = PN_SASL_AUTH
    SYS = PN_SASL_SYS
    PERM = PN_SASL_PERM
    TEMP = PN_SASL_TEMP

    @staticmethod
    def extended() -> bool:
        """
        Check for support of extended SASL negotiation.

        All implementations of Proton support ``ANONYMOUS`` and ``EXTERNAL`` on both
        client and server sides and ``PLAIN`` on the client side.

        Extended SASL implementations use an external library (Cyrus SASL)
        to support other mechanisms beyond these basic ones.

        :rtype: ``True`` if we support extended SASL negotiation, ``False`` if
               we only support basic negotiation.
        """
        return pn_sasl_extended()

    def __init__(self, transport: Transport) -> None:
        Wrapper.__init__(self, transport._impl, pn_transport_attachments)
        self._sasl = pn_sasl(transport._impl)

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, SASLException)
            raise exc("[%s]" % (err))
        else:
            return err

    @property
    def user(self) -> Optional[str]:
        """
        Retrieve the authenticated user. This is usually used at the the
        server end to find the name of the authenticated user.

        If :meth:`outcome` returns a value other than :const:`OK`, then
        there will be no user to return. The returned value is only reliable
        after the ``PN_TRANSPORT_AUTHENTICATED`` event has been received.

        :rtype: * If the SASL layer was not negotiated then ``None`` is returned.
                * If the ``ANONYMOUS`` mechanism is used then the user will be
                  ``"anonymous"``.
                * Otherwise a string containing the user is
                  returned.
        """
        return pn_sasl_get_user(self._sasl)

    @property
    def authorization(self) -> Optional[str]:
        """
        Retrieve the requested authorization user. This is usually used at the the
        server end to find the name of any requested authorization user.

        If the peer has not requested an authorization user or the SASL mechanism has
        no capability to transport an authorization id this will be the same as the
        authenticated user.

        Note that it is the role of the server to ensure that the authenticated user is
        actually allowed to act as the requested authorization user.

        If :meth:`outcome` returns a value other than :const:`OK`, then
        there will be no user to return. The returned value is only reliable
        after the ``PN_TRANSPORT_AUTHENTICATED`` event has been received.

        :rtype: * If the SASL layer was not negotiated then ``None`` is returned.
                * If the ``ANONYMOUS`` mechanism is used then the user will be
                  ``"anonymous"``.
                * Otherwise a string containing the user is
                  returned.
        """
        return pn_sasl_get_authorization(self._sasl)

    @property
    def mech(self) -> str:
        """
        Return the selected SASL mechanism.

        The returned value is only reliable after the ``PN_TRANSPORT_AUTHENTICATED``
        event has been received.

        :rtype: The authentication mechanism selected by the SASL layer.
        """
        return pn_sasl_get_mech(self._sasl)

    @property
    def outcome(self) -> Optional[int]:
        """
        Retrieve the outcome of SASL negotiation.

        :rtype: * ``None`` if no negotiation has taken place.
                * Otherwise the outcome of the negotiation.
        """
        outcome = pn_sasl_outcome(self._sasl)
        if outcome == PN_SASL_NONE:
            return None
        else:
            return outcome

    def allowed_mechs(self, mechs: Union[str, List[str]]) -> None:
        """
        SASL mechanisms that are to be considered for authentication.

        This can be used on either the client or the server to restrict
        the SASL mechanisms that may be used to the mechanisms on the list.

        **NOTE:** By default the ``GSSAPI`` and ``GSS-SPNEGO`` mechanisms
        are not enabled for clients. This is because these mechanisms have
        the problematic behaviour of 'capturing' the client whenever they
        are installed so that they will be used by the client if offered by
        the server even if the client can't successfully authenticate this
        way. This can lead to some very hard to debug failures.

        **NOTE:** The ``GSSAPI`` or ``GSS-SPNEGO`` mechanisms need to be
        explicitly enabled if they are required (together with any other
        required mechanisms).

        :param mechs: A list of mechanisms that are allowed for authentication,
                      either a string containing a space-separated list of mechs
                      ``"mech1 mech2 ..."``, or a Python list of strings
                      ``["mech1", "mech2", ...]``.
        """
        if isinstance(mechs, list):
            mechs = " ".join(mechs)
        pn_sasl_allowed_mechs(self._sasl, mechs)

    @property
    def allow_insecure_mechs(self) -> bool:
        """Allow unencrypted cleartext passwords (PLAIN mech)"""
        return pn_sasl_get_allow_insecure_mechs(self._sasl)

    @allow_insecure_mechs.setter
    def allow_insecure_mechs(self, insecure: bool) -> None:
        pn_sasl_set_allow_insecure_mechs(self._sasl, insecure)

    def done(self, outcome):
        """
        Set the outcome of SASL negotiation. Used by the server to set the
        result of the negotiation process.
        """
        pn_sasl_done(self._sasl, outcome)

    def config_name(self, name: str):
        """
        Set the SASL configuration name. This is used to construct the SASL
        configuration filename. In the current implementation ``".conf"`` is
        added to the name and the file is looked for in the configuration
        directory.

        If not set it will default to ``"proton-server"`` for a sasl server
        and ``"proton-client"`` for a client.

        :param name: The configuration name.
        """
        pn_sasl_config_name(self._sasl, name)

    def config_path(self, path: str):
        """
        Set the SASL configuration path. This is used to tell SASL where
        to look for the configuration file. In the current implementation
        it can be a colon separated list of directories.

        The environment variable ``PN_SASL_CONFIG_PATH`` can also be used
        to set this path, but if both methods are used then this
        :meth:`config_path` will take precedence.

        If not set, the underlying implementation default will be used.

        :param path: The configuration path, may contain colon-separated list
                     if more than one path is specified.
        """
        pn_sasl_config_path(self._sasl, path)


class SSLDomain(object):
    """
    An SSL configuration domain, used to hold the SSL configuration
    for one or more SSL sessions.
    """

    MODE_CLIENT = PN_SSL_MODE_CLIENT
    """Local connection endpoint is an SSL client."""

    MODE_SERVER = PN_SSL_MODE_SERVER
    """Local connection endpoint is an SSL server."""

    VERIFY_PEER = PN_SSL_VERIFY_PEER
    """Require peer to provide a valid identifying certificate."""

    VERIFY_PEER_NAME = PN_SSL_VERIFY_PEER_NAME
    """Require valid certificate and matching name."""

    ANONYMOUS_PEER = PN_SSL_ANONYMOUS_PEER
    """Do not require a certificate nor cipher authorization."""

    def __init__(self, mode: int) -> None:
        self._domain = pn_ssl_domain(mode)
        if self._domain is None:
            raise SSLUnavailable()

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, SSLException)
            raise exc("SSL failure.")
        else:
            return err

    def set_credentials(self, cert_file: str, key_file: str, password: Optional[str]) -> int:
        """
        Set the certificate that identifies the local node to the remote.

        This certificate establishes the identity for the local node for all :class:`SSL` sessions
        created from this domain.  It will be sent to the remote if the remote needs to verify
        the identity of this node.  This may be used for both SSL servers and SSL clients (if
        client authentication is required by the server).

        .. note:: This setting effects only those :class:`SSL` objects created after this call
            returns.  :class:`SSL` objects created before invoking this method will use the domain's
            previous setting.

        :param cert_file: Specifier for the file/database containing the identifying
               certificate. For Openssl users, this is a PEM file. For Windows SChannel
               users, this is the PKCS#12 file or system store.
        :param key_file: An optional key to access the identifying certificate. For
               Openssl users, this is an optional PEM file containing the private key
               used to sign the certificate. For Windows SChannel users, this is the
               friendly name of the self-identifying certificate if there are multiple
               certificates in the store.
        :param password: The password used to sign the key, else ``None`` if key is not
               protected.
        :return: 0 on success
        :raise: :exc:`SSLException` if there is any Proton error
        """
        return self._check(pn_ssl_domain_set_credentials(self._domain,
                                                         cert_file, key_file,
                                                         password))

    def set_trusted_ca_db(self, certificate_db: str) -> int:
        """
        Configure the set of trusted CA certificates used by this domain to verify peers.

        If the local SSL client/server needs to verify the identity of the remote, it must
        validate the signature of the remote's certificate.  This function sets the database of
        trusted CAs that will be used to verify the signature of the remote's certificate.

        .. note:: This setting effects only those :class:`SSL` objects created after this call
            returns.  :class:`SSL` objects created before invoking this method will use the domain's
            previous setting.

        .. note:: By default the list of trusted CA certificates will be set to the system default.
            What this is is depends on the OS and the SSL implementation used: For OpenSSL the default
            will depend on how the OS is set up. When using the Windows SChannel implementation the default
            will be the users default trusted certificate store.

        :param certificate_db: Database of trusted CAs, used to authenticate the peer.
        :return: 0 on success
        :raise: :exc:`SSLException` if there is any Proton error
        """
        return self._check(pn_ssl_domain_set_trusted_ca_db(self._domain,
                                                           certificate_db))

    def set_peer_authentication(self, verify_mode: int, trusted_CAs: Optional[str] = None) -> int:
        """
        This method controls how the peer's certificate is validated, if at all.  By default,
        servers do not attempt to verify their peers (PN_SSL_ANONYMOUS_PEER) but
        clients attempt to verify both the certificate and peer name (PN_SSL_VERIFY_PEER_NAME).
        Once certificates and trusted CAs are configured, peer verification can be enabled.

        .. note:: In order to verify a peer, a trusted CA must be configured. See
            :meth:`set_trusted_ca_db`.

        .. note:: Servers must provide their own certificate when verifying a peer.  See
            :meth:`set_credentials`.

        .. note:: This setting effects only those :class:`SSL` objects created after this call
            returns.  :class:`SSL` objects created before invoking this method will use the domain's
            previous setting.

        :param verify_mode: The level of validation to apply to the peer, one of :const:`VERIFY_PEER`,
                            :const:`VERIFY_PEER_NAME`, :const:`ANONYMOUS_PEER`,
        :param trusted_CAs: Path to a database of trusted CAs that the server will advertise.
        :return: 0 on success
        :raise: :exc:`SSLException` if there is any Proton error
        """
        return self._check(pn_ssl_domain_set_peer_authentication(self._domain,
                                                                 verify_mode,
                                                                 trusted_CAs))

    def allow_unsecured_client(self) -> int:
        """
        Permit a server to accept connection requests from non-SSL clients.

        This configures the server to "sniff" the incoming client data stream,
        and dynamically determine whether SSL/TLS is being used.  This option
        is disabled by default: only clients using SSL/TLS are accepted.

        :raise: :exc:`SSLException` if there is any Proton error
        """
        return self._check(pn_ssl_domain_allow_unsecured_client(self._domain))

    def __del__(self) -> None:
        pn_ssl_domain_free(self._domain)


class SSL(object):
    """
    An SSL session associated with a transport. A transport must have
    an SSL object in order to "speak" SSL over its connection.
    """

    @staticmethod
    def present() -> bool:
        """
        Tests for an SSL implementation being present.

        :return: ``True`` if we support SSL, ``False`` if not.
        """
        return pn_ssl_present()

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, SSLException)
            raise exc("SSL failure.")
        else:
            return err

    def __new__(
            cls: Type['SSL'],
            transport: Transport,
            domain: SSLDomain,
            session_details: Optional['SSLSessionDetails'] = None
    ) -> 'SSL':
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

    def cipher_name(self) -> Optional[str]:
        """
        Get the name of the Cipher that is currently in use.

        Gets a text description of the cipher that is currently active, or
        returns ``None`` if SSL is not active (no cipher).

        .. note:: The cipher in use may change over time due to renegotiation
            or other changes to the SSL state.

        :return: The cypher name, or ``None`` if no cipher in use.
        """
        return pn_ssl_get_cipher_name(self._ssl, 128)

    def protocol_name(self) -> Optional[str]:
        """
        Get the name of the SSL protocol that is currently in use.

        Gets a text description of the SSL protocol that is currently active,
        or returns ``None`` if SSL is not active.

        .. note:: The protocol may change over time due to renegotiation.

        :return: The protocol name if SSL is active, or ``None`` if SSL connection
                 is not ready or active.
        """
        return pn_ssl_get_protocol_name(self._ssl, 128)

    SHA1 = PN_SSL_SHA1
    """Produces hash that is 20 bytes long using SHA-1"""

    SHA256 = PN_SSL_SHA256
    """Produces hash that is 32 bytes long using SHA-256"""

    SHA512 = PN_SSL_SHA512
    """Produces hash that is 64 bytes long using SHA-512"""

    MD5 = PN_SSL_MD5
    """Produces hash that is 16 bytes long using MD5"""

    CERT_COUNTRY_NAME = PN_SSL_CERT_SUBJECT_COUNTRY_NAME
    """Certificate country name 2-char ISO code"""

    CERT_STATE_OR_PROVINCE = PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE
    """Certificate state or province, not abbreviated"""

    CERT_CITY_OR_LOCALITY = PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY
    """Certificate city or place name, not abbreviated"""

    CERT_ORGANIZATION_NAME = PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME
    """Certificate organization name"""

    CERT_ORGANIZATION_UNIT = PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT
    """Certificate organization unit or division within organization"""

    CERT_COMMON_NAME = PN_SSL_CERT_SUBJECT_COMMON_NAME
    """Certificate common name or URL"""

    def get_cert_subject_subfield(self, subfield_name: int) -> Optional[str]:
        """
        Returns a string that contains the value of the sub field of
        the subject field in the ssl certificate. The subject field
        usually contains the following values:

            * :const:`CERT_COUNTRY_NAME`
            * :const:`CERT_STATE_OR_PROVINCE`
            * :const:`CERT_CITY_OR_LOCALITY`
            * :const:`CERT_ORGANIZATION_NAME`
            * :const:`CERT_ORGANIZATION_UNIT`
            * :const:`CERT_COMMON_NAME`

        :param subfield_name: The enumeration representing the required
                              sub field listed above
        :return: A string which contains the requested sub field value which
                 is valid until the ssl object is destroyed.
        """
        subfield_value = pn_ssl_get_remote_subject_subfield(self._ssl, subfield_name)
        return subfield_value

    def get_cert_subject(self) -> str:
        """
        Get the subject from the peer's certificate.

        :return: A string containing the full subject.
        """
        subject = pn_ssl_get_remote_subject(self._ssl)
        return subject

    def _get_cert_subject_unknown_subfield(self) -> None:
        # Pass in an unhandled enum
        return self.get_cert_subject_subfield(10)

    # Convenience functions for obtaining the subfields of the subject field.
    def get_cert_common_name(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_COMMON_NAME`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_COMMON_NAME` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_COMMON_NAME)

    def get_cert_organization(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_ORGANIZATION_NAME`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_ORGANIZATION_NAME` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_ORGANIZATION_NAME)

    def get_cert_organization_unit(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_ORGANIZATION_UNIT`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_ORGANIZATION_UNIT` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_ORGANIZATION_UNIT)

    def get_cert_locality_or_city(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_CITY_OR_LOCALITY`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_CITY_OR_LOCALITY` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_CITY_OR_LOCALITY)

    def get_cert_country(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_COUNTRY_NAME`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_COUNTRY_NAME` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_COUNTRY_NAME)

    def get_cert_state_or_province(self) -> str:
        """
        A convenience method to get a string that contains the :const:`CERT_STATE_OR_PROVINCE`
        sub field of the subject field in the ssl certificate.

        :return: A string containing the :const:`CERT_STATE_OR_PROVINCE` sub field.
        """
        return self.get_cert_subject_subfield(SSL.CERT_STATE_OR_PROVINCE)

    def get_cert_fingerprint(self, fingerprint_length: int, digest_name: int) -> Optional[str]:
        """
        Get the fingerprint of the certificate. The certificate fingerprint
        (as displayed in the Fingerprints section when looking at a certificate
        with say the Firefox browser) is the hexadecimal hash of the entire
        certificate. The fingerprint is not part of the certificate, rather
        it is computed from the certificate and can be used to uniquely identify
        a certificate.

        :param fingerprint_length: Must be :math:`>= 33` for md5, :math:`>= 41`
                                   for sha1, :math:`>= 65` for sha256 and :math:`>= 129`
                                   for sha512.
        :param digest_name: The hash algorithm to use. Must be one of :const:`SHA1`,
                            :const:`SHA256`, :const:`SHA512`,  :const:`MD5`.
        :return: Hex fingerprint in a string, or ``None`` if an error occurred.
        """
        return pn_ssl_get_cert_fingerprint(self._ssl, fingerprint_length, digest_name)

    # Convenience functions for obtaining fingerprint for specific hashing algorithms
    def _get_cert_fingerprint_unknown_hash_alg(self) -> None:
        return self.get_cert_fingerprint(41, 10)

    def get_cert_fingerprint_sha1(self) -> Optional[str]:
        """
        A convenience method to get the :const:`SHA1` fingerprint of the
        certificate.

        :return: Hex fingerprint in a string, or ``None`` if an error occurred.
        """
        return self.get_cert_fingerprint(41, SSL.SHA1)

    def get_cert_fingerprint_sha256(self) -> Optional[str]:
        """
        A convenience method to get the :const:`SHA256` fingerprint of the
        certificate.

        :return: Hex fingerprint in a string, or ``None`` if an error occurred.
        """
        # sha256 produces a fingerprint that is 64 characters long
        return self.get_cert_fingerprint(65, SSL.SHA256)

    def get_cert_fingerprint_sha512(self) -> Optional[str]:
        """
        A convenience method to get the :const:`SHA512` fingerprint of the
        certificate.

        :return: Hex fingerprint in a string, or ``None`` if an error occurred.
        """
        # sha512 produces a fingerprint that is 128 characters long
        return self.get_cert_fingerprint(129, SSL.SHA512)

    def get_cert_fingerprint_md5(self) -> Optional[str]:
        """
        A convenience method to get the :const:`MD5` fingerprint of the
        certificate.

        :return: Hex fingerprint in a string, or ``None`` if an error occurred.
        """
        return self.get_cert_fingerprint(33, SSL.MD5)

    @property
    def remote_subject(self) -> str:
        """
        The subject from the peers certificate.
        """
        return pn_ssl_get_remote_subject(self._ssl)

    RESUME_UNKNOWN = PN_SSL_RESUME_UNKNOWN
    """Session resume state unknown/not supported."""

    RESUME_NEW = PN_SSL_RESUME_NEW
    """Session renegotiated - not resumed."""

    RESUME_REUSED = PN_SSL_RESUME_REUSED
    """Session resumed from previous session."""

    def resume_status(self) -> int:
        """
        Check whether the state has been resumed.

        Used for client session resume.  When called on an active session,
        indicates whether the state has been resumed from a previous session.

        .. note:: This is a best-effort service - there is no guarantee that
            the remote server will accept the resumed parameters.  The remote
            server may choose to ignore these parameters, and request a
            re-negotiation instead.

        :return: Status code indicating whether or not the session has been
                 resumed. One of:
                 * :const:`RESUME_UNKNOWN`
                 * :const:`RESUME_NEW`
                 * :const:`RESUME_REUSED`
        """
        return pn_ssl_resume_status(self._ssl)

    @property
    def peer_hostname(self) -> str:
        """Manage the expected name of the remote peer.

        The hostname is used for two purposes:

            1. when set on an SSL client, it is sent to the server during the
               handshake (if Server Name Indication is supported)
            2. it is used to check against the identifying name provided in the
               peer's certificate. If the supplied name does not exactly match a
               SubjectAltName (type DNS name), or the CommonName entry in the
               peer's certificate, the peer is considered unauthenticated
               (potential imposter), and the SSL connection is aborted.

        .. note:: Verification of the hostname is only done if
            :const:`SSLDomain.VERIFY_PEER_NAME` is set using
            :meth:`SSLDomain.set_peer_authentication`."""
        err, name = pn_ssl_get_peer_hostname(self._ssl, 1024)
        self._check(err)
        return name

    @peer_hostname.setter
    def peer_hostname(self, hostname: Optional[str]) -> None:
        self._check(pn_ssl_set_peer_hostname(self._ssl, hostname))


class SSLSessionDetails(object):
    """
    Unique identifier for the SSL session.  Used to resume previous
    session on a new SSL connection.
    """

    def __init__(self, session_id: str) -> None:
        self._session_id = session_id

    def get_session_id(self) -> str:
        """
        Get the unique identifier for this SSL session

        :return: Session identifier
        """
        return self._session_id
