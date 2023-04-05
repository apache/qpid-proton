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

"""
The proton.endpoints module
"""

import weakref

from cproton import PN_CONFIGURATION, PN_COORDINATOR, PN_DELIVERIES, PN_DIST_MODE_COPY, PN_DIST_MODE_MOVE, \
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
    pn_terminus_set_expiry_policy, pn_terminus_set_timeout, pn_terminus_set_type, \
    pn_link_properties, pn_link_remote_properties, \
    isnull

from ._condition import cond2obj, obj2cond
from ._data import Data, dat2obj, obj2dat, PropertyDict, SymbolList
from ._delivery import Delivery
from ._exceptions import ConnectionException, EXCEPTIONS, LinkException, SessionException
from ._handler import Handler
from ._transport import Transport
from ._wrapper import Wrapper
from typing import Dict, List, Optional, Union, TYPE_CHECKING, Any

if TYPE_CHECKING:
    from ._condition import Condition
    from ._data import Array, PythonAMQPData, symbol
    from ._events import Collector
    from ._message import Message


class Endpoint(object):
    """
    Abstract class from which :class:`Connection`, :class:`Session`
    and :class:`Link` are derived, and which defines the state
    of these classes.

    The :class:`Endpoint` state is an integral value with flags that
    encode both the local and remote state of an AMQP Endpoint
    (:class:`Connection`, :class:`Link`, or :class:`Session`).
    The individual bits may be accessed using :const:`LOCAL_UNINIT`,
    :const:`LOCAL_ACTIVE`, :const:`LOCAL_CLOSED`, and
    :const:`REMOTE_UNINIT`, :const:`REMOTE_ACTIVE`, :const:`REMOTE_CLOSED`.

    Every AMQP endpoint (:class:`Connection`, :class:`Link`, or
    :class:`Session`) starts out in an uninitialized state and then
    proceeds linearly to an active and then closed state. This
    lifecycle occurs at both endpoints involved, and so the state
    model for an endpoint includes not only the known local state,
    but also the last known state of the remote endpoint.
    """

    LOCAL_UNINIT = PN_LOCAL_UNINIT
    """ The local  endpoint state is uninitialized. """

    REMOTE_UNINIT = PN_REMOTE_UNINIT
    """ The local endpoint state is active. """

    LOCAL_ACTIVE = PN_LOCAL_ACTIVE
    """ The local endpoint state is closed. """

    REMOTE_ACTIVE = PN_REMOTE_ACTIVE
    """ The remote endpoint state is uninitialized. """

    LOCAL_CLOSED = PN_LOCAL_CLOSED
    """ The remote endpoint state is active. """

    REMOTE_CLOSED = PN_REMOTE_CLOSED
    """ The remote endpoint state is closed. """

    def _init(self) -> None:
        self.condition: Optional['Condition'] = None
        self._handler: Optional[Handler] = None

    def _update_cond(self) -> None:
        obj2cond(self.condition, self._get_cond_impl())

    @property
    def remote_condition(self) -> Optional['Condition']:
        """
        The remote condition associated with the connection endpoint.
        See :class:`Condition` for more information.
        """
        return cond2obj(self._get_remote_cond_impl())

    # the following must be provided by subclasses
    def _get_cond_impl(self):
        assert False, "Subclass must override this!"

    def _get_remote_cond_impl(self):
        assert False, "Subclass must override this!"

    @property
    def handler(self) -> Optional[Handler]:
        """Handler for events.

        :getter: Get the event handler, or return ``None`` if no handler has been set.
        :setter: Set the event handler."""
        return self._handler

    @handler.setter
    def handler(self, handler: Optional[Handler]) -> None:
        # TODO Hack This is here for some very odd (IMO) backwards compat behaviour
        if handler is None:
            self._handler = None
        elif isinstance(handler, Handler):
            self._handler = handler
        else:
            self._handler = Handler()
            self._handler.add(handler)


class Connection(Wrapper, Endpoint):
    """
    A representation of an AMQP connection.
    """

    @staticmethod
    def wrap(impl):
        if isnull(impl):
            return None
        else:
            return Connection(impl)

    def __init__(self, impl: Any = None) -> None:
        if impl is None:
            Wrapper.__init__(self, constructor=pn_connection, get_context=pn_connection_attachments)
        else:
            Wrapper.__init__(self, impl, pn_connection_attachments)

    def _init(self) -> None:
        Endpoint._init(self)
        self.offered_capabilities_list = None
        self.desired_capabilities_list = None
        self.properties = None
        self.url = None
        self._acceptor = None

    def _get_attachments(self):
        return pn_connection_attachments(self._impl)

    @property
    def connection(self) -> 'Connection':
        """
        Get this connection.
        """
        return self

    @property
    def transport(self) -> Optional[Transport]:
        """
        The transport bound to this connection. If the connection
        is unbound, then this operation will return ``None``.
        """
        return Transport.wrap(pn_connection_transport(self._impl))

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, ConnectionException)
            raise exc("[%s]: %s" % (err, pn_connection_error(self._impl)))
        else:
            return err

    def _get_cond_impl(self):
        return pn_connection_condition(self._impl)

    def _get_remote_cond_impl(self):
        return pn_connection_remote_condition(self._impl)

    # TODO: Blacklisted API call
    def collect(self, collector: 'Collector') -> None:
        if collector is None:
            pn_connection_collect(self._impl, None)
        else:
            pn_connection_collect(self._impl, collector._impl)
        self._collector = weakref.ref(collector)

    @property
    def container(self) -> str:
        """The container name for this connection object."""
        return pn_connection_get_container(self._impl)

    @container.setter
    def container(self, name: str) -> None:
        pn_connection_set_container(self._impl, name)

    @property
    def hostname(self) -> Optional[str]:
        """Set the name of the host (either fully qualified or relative) to which this
        connection is connecting to.  This information may be used by the remote
        peer to determine the correct back-end service to connect the client to.
        This value will be sent in the Open performative, and will be used by SSL
        and SASL layers to identify the peer.
        """
        return pn_connection_get_hostname(self._impl)

    @hostname.setter
    def hostname(self, name: str) -> None:
        pn_connection_set_hostname(self._impl, name)

    @property
    def user(self) -> Optional[str]:
        """The authentication username for a client connection.

        It is necessary to set the username and password before binding
        the connection to a transport and it isn't allowed to change
        after the binding.

        If not set then no authentication will be negotiated unless the
        client sasl layer is explicitly created (this would be for something
        like Kerberos where the credentials are implicit in the environment,
        or to explicitly use the ``ANONYMOUS`` SASL mechanism)
        """
        return pn_connection_get_user(self._impl)

    @user.setter
    def user(self, name: str) -> None:
        pn_connection_set_user(self._impl, name)

    @property
    def authorization(self) -> str:
        """The authorization username for a client connection.

        It is necessary to set the authorization before binding
        the connection to a transport and it isn't allowed to change
        after the binding.

        If not set then implicitly the requested authorization is the same as the
        authentication user.
        """
        return pn_connection_get_authorization(self._impl)

    @authorization.setter
    def authorization(self, name: str) -> None:
        pn_connection_set_authorization(self._impl, name)

    @property
    def password(self) -> None:
        """Set the authentication password for a client connection.

        It is necessary to set the username and password before binding the connection
        to a transport and it isn't allowed to change after the binding.

        .. note:: Getting the password always returns ``None``.
        """
        return None

    @password.setter
    def password(self, name: str) -> None:
        pn_connection_set_password(self._impl, name)

    @property
    def remote_container(self) -> Optional[str]:
        """
        The container identifier specified by the remote peer for this connection.

        This will return ``None`` until the :const:'REMOTE_ACTIVE` state is
        reached. See :class:`Endpoint` for more details on endpoint state.

        Any (non ``None``) name returned by this operation will be valid until
        the connection object is unbound from a transport or freed,
        whichever happens sooner.
        """
        return pn_connection_remote_container(self._impl)

    @property
    def remote_hostname(self) -> Optional[str]:
        """
        The hostname specified by the remote peer for this connection.

        This will return ``None`` until the :const:`REMOTE_ACTIVE` state is
        reached. See :class:`Endpoint` for more details on endpoint state.

        Any (non ``None``) name returned by this operation will be valid until
        the connection object is unbound from a transport or freed,
        whichever happens sooner.
        """
        return pn_connection_remote_hostname(self._impl)

    @property
    def remote_offered_capabilities(self):
        """
        The capabilities offered by the remote peer for this connection.

        This operation will return a :class:`Data` object that
        is valid until the connection object is freed. This :class:`Data`
        object will be empty until the remote connection is opened as
        indicated by the :const:`REMOTE_ACTIVE` flag.

        :type: :class:`Data`
        """
        c = dat2obj(pn_connection_remote_offered_capabilities(self._impl))
        return c and SymbolList(c)

    @property
    def remote_desired_capabilities(self):
        """
        The capabilities desired by the remote peer for this connection.

        This operation will return a :class:`Data` object that
        is valid until the connection object is freed. This :class:`Data`
        object will be empty until the remote connection is opened as
        indicated by the :const:`REMOTE_ACTIVE` flag.

        :type: :class:`Data`
        """
        c = dat2obj(pn_connection_remote_desired_capabilities(self._impl))
        return c and SymbolList(c)

    @property
    def remote_properties(self):
        """
        The properties specified by the remote peer for this connection.

        This operation will return a :class:`Data` object that
        is valid until the connection object is freed. This :class:`Data`
        object will be empty until the remote connection is opened as
        indicated by the :const:`REMOTE_ACTIVE` flag.

        :type: :class:`Data`
        """
        return dat2obj(pn_connection_remote_properties(self._impl))

    @property
    def connected_address(self) -> str:
        """The address for this connection."""
        return self.url and str(self.url)

    def open(self) -> None:
        """
        Opens the connection.

        In more detail, this moves the local state of the connection to
        the ``ACTIVE`` state and triggers an open frame to be sent to the
        peer. A connection is fully active once both peers have opened it.
        """
        obj2dat(self.offered_capabilities,
                pn_connection_offered_capabilities(self._impl))
        obj2dat(self.desired_capabilities,
                pn_connection_desired_capabilities(self._impl))
        obj2dat(self.properties, pn_connection_properties(self._impl))
        pn_connection_open(self._impl)

    def close(self) -> None:
        """
        Closes the connection.

        In more detail, this moves the local state of the connection to
        the ``CLOSED`` state and triggers a close frame to be sent to the
        peer. A connection is fully closed once both peers have closed it.
        """
        self._update_cond()
        pn_connection_close(self._impl)
        if hasattr(self, '_session_policy'):
            # break circular ref
            del self._session_policy
        t = self.transport
        if t and t._connect_selectable:
            # close() requested before TCP connect handshake completes on socket.
            # Dismantle connection setup logic.
            s = t._connect_selectable
            t._connect_selectable = None
            t.close_head()
            t.close_tail()
            s._transport = None
            t._selectable = None
            s.terminate()
            s.update()

    @property
    def state(self) -> int:
        """
        The state of the connection as a bit field. The state has a local
        and a remote component. Each of these can be in one of three
        states: ``UNINIT``, ``ACTIVE`` or ``CLOSED``. These can be tested by masking
        against :const:`LOCAL_UNINIT`, :const:`LOCAL_ACTIVE`, :const:`LOCAL_CLOSED`, :const:`REMOTE_UNINIT`,
        :const:`REMOTE_ACTIVE` and :const:`REMOTE_CLOSED`.
        """
        return pn_connection_state(self._impl)

    def session(self) -> 'Session':
        """
        Returns a new session on this connection.

        :return: New session
        :raises: :class:`SessionException`
        """
        ssn = pn_session(self._impl)
        if ssn is None:
            raise (SessionException("Session allocation failed."))
        else:
            return Session(ssn)

    def session_head(self, mask: int) -> Optional['Session']:
        """
        Retrieve the first session from a given connection that matches the
        specified state mask.

        Examines the state of each session owned by the connection, and
        returns the first session that matches the given state mask. If
        state contains both local and remote flags, then an exact match
        against those flags is performed. If state contains only local or
        only remote flags, then a match occurs if any of the local or
        remote flags are set respectively.

        :param mask: State mask to match
        :return: The first session owned by the connection that matches the
            mask, else ``None`` if no sessions matches.
        """
        return Session.wrap(pn_session_head(self._impl, mask))

    def link_head(self, mask: int) -> Optional[Union['Sender', 'Receiver']]:
        """
        Retrieve the first link that matches the given state mask.

        Examines the state of each link owned by the connection and returns
        the first link that matches the given state mask. If state contains
        both local and remote flags, then an exact match against those
        flags is performed. If state contains only local or only remote
        flags, then a match occurs if any of the local or remote flags are
        set respectively. ``state==0`` matches all links.

        :param mask: State mask to match
        :return: The first link owned by the connection that matches the
            mask, else ``None`` if no link matches.
        """
        return Link.wrap(pn_link_head(self._impl, mask))

    @property
    def error(self):
        """
        Additional error information associated with the connection.

        Whenever a connection operation fails (i.e. returns an error code),
        additional error details can be obtained using this property. The
        returned value is the error code defined by Proton in ``pn_error_t``
        (see ``error.h``).

        :type: ``int``
        """
        return pn_error_code(pn_connection_error(self._impl))

    def free(self) -> None:
        """
        Releases this connection object.

        When a connection object is released, all :class:`Session` and
        :class:`Link` objects associated with this connection are also
        released and all :class:`Delivery` objects are settled.
        """
        pn_connection_release(self._impl)

    @property
    def offered_capabilities(self) -> Optional[Union['Array', SymbolList]]:
        """Offered capabilities as a list of symbols. The AMQP 1.0 specification
        restricts this list to symbol elements only. It is possible to use
        the special ``list`` subclass :class:`SymbolList` as it will by
        default enforce this restriction on construction. In addition, if a
        string type is used, it will be silently converted into the required
        symbol.
        """
        return self.offered_capabilities_list

    @offered_capabilities.setter
    def offered_capabilities(
            self,
            offered_capability_list: Optional[Union['Array', List['symbol'], SymbolList, List[str]]]
    ) -> None:
        self.offered_capabilities_list = SymbolList(offered_capability_list)

    @property
    def desired_capabilities(self) -> Optional[Union['Array', SymbolList]]:
        """Desired capabilities as a list of symbols. The AMQP 1.0 specification
        restricts this list to symbol elements only. It is possible to use
        the special ``list`` subclass :class:`SymbolList` which will by
        default enforce this restriction on construction. In addition, if string
        types are used, this class will be silently convert them into symbols.
        """
        return self.desired_capabilities_list

    @desired_capabilities.setter
    def desired_capabilities(
            self,
            desired_capability_list: Optional[Union['Array', List['symbol'], SymbolList, List[str]]]
    ) -> None:
        self.desired_capabilities_list = SymbolList(desired_capability_list)

    @property
    def properties(self) -> Optional[PropertyDict]:
        """Connection properties as a dictionary of key/values. The AMQP 1.0
        specification restricts this dictionary to have keys that are only
        :class:`symbol` types. It is possible to use the special ``dict``
        subclass :class:`PropertyDict` which will by default enforce this
        restrictions on construction. In addition, if strings type are used,
        this will silently convert them into symbols.
        """
        return self.properties_dict

    @properties.setter
    def properties(self, properties_dict: Optional[Union[PropertyDict, Dict[str, 'PythonAMQPData']]]) -> None:
        if isinstance(properties_dict, dict):
            self.properties_dict = PropertyDict(properties_dict, raise_on_error=False)
        else:
            self.properties_dict = properties_dict


class Session(Wrapper, Endpoint):
    """A container of links"""
    @staticmethod
    def wrap(impl):
        if isnull(impl):
            return None
        else:
            return Session(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_session_attachments)

    def _get_attachments(self):
        return pn_session_attachments(self._impl)

    def _get_cond_impl(self):
        return pn_session_condition(self._impl)

    def _get_remote_cond_impl(self):
        return pn_session_remote_condition(self._impl)

    @property
    def incoming_capacity(self) -> int:
        """The incoming capacity of this session in bytes. The incoming capacity
        of a session determines how much incoming message data the session
        can buffer.

        .. note:: If set, this value must be greater than or equal to the negotiated
            frame size of the transport. The window is computed as a whole number of
            frames when dividing remaining capacity at a given time by the connection
            max frame size. As such, capacity and max frame size should be chosen so
            as to ensure the frame window isn't unduly small and limiting performance.
        """
        return pn_session_get_incoming_capacity(self._impl)

    @incoming_capacity.setter
    def incoming_capacity(self, capacity: int) -> None:
        pn_session_set_incoming_capacity(self._impl, capacity)

    @property
    def outgoing_window(self) -> int:
        """The outgoing window for this session."""
        return pn_session_get_outgoing_window(self._impl)

    @outgoing_window.setter
    def outgoing_window(self, window: int) -> None:
        pn_session_set_outgoing_window(self._impl, window)

    @property
    def outgoing_bytes(self) -> int:
        """
        The number of outgoing bytes currently buffered."""
        return pn_session_outgoing_bytes(self._impl)

    @property
    def incoming_bytes(self) -> int:
        """
        The number of incoming bytes currently buffered.
        """
        return pn_session_incoming_bytes(self._impl)

    def open(self) -> None:
        """
        Open a session. Once this operation has completed, the
        :const:`LOCAL_ACTIVE` state flag will be set.
        """
        pn_session_open(self._impl)

    def close(self) -> None:
        """
        Close a session.

        Once this operation has completed, the :const:`LOCAL_CLOSED` state flag
        will be set. This may be called without calling
        :meth:`open`, in this case it is equivalent to calling
        :meth:`open` followed by :meth:`close`.

        """
        self._update_cond()
        pn_session_close(self._impl)

    def next(self, mask):
        """
        Retrieve the next session for this connection that matches the
        specified state mask.

        When used with :meth:`Connection.session_head`, application can
        access all sessions on the connection that match the given state.
        See :meth:`Connection.session_head` for description of match
        behavior.

        :param mask: Mask to match.
        :return: The next session owned by this connection that matches the
            mask, else ``None`` if no sessions match.
        :rtype: :class:`Session` or ``None``
        """
        return Session.wrap(pn_session_next(self._impl, mask))

    @property
    def state(self) -> int:
        """
        The endpoint state flags for this session. See :class:`Endpoint` for
        details of the flags.
        """
        return pn_session_state(self._impl)

    @property
    def connection(self) -> Connection:
        """
        The parent connection for this session.
        """
        return Connection.wrap(pn_session_connection(self._impl))

    @property
    def transport(self) -> Transport:
        """
        The transport bound to the parent connection for this session.
        """
        return self.connection.transport

    def sender(self, name: str) -> 'Sender':
        """
        Create a new :class:`Sender` on this session.

        :param name: Name of sender
        """
        return Sender(pn_sender(self._impl, name))

    def receiver(self, name: str) -> 'Receiver':
        """
        Create a new :class:`Receiver` on this session.

        :param name: Name of receiver
        """
        return Receiver(pn_receiver(self._impl, name))

    def free(self) -> None:
        """
        Free this session. When a session is freed it will no
        longer be retained by the connection once any internal
        references to the session are no longer needed. Freeing
        a session will free all links on that session and settle
        any deliveries on those links.
        """
        pn_session_free(self._impl)


class Link(Wrapper, Endpoint):
    """
    A representation of an AMQP link (a unidirectional channel for
    transferring messages), of which there are two concrete
    implementations, :class:`Sender` and :class:`Receiver`.
    """

    SND_UNSETTLED = PN_SND_UNSETTLED
    """The sender will send all deliveries initially unsettled."""
    SND_SETTLED = PN_SND_SETTLED
    """The sender will send all deliveries settled to the receiver."""
    SND_MIXED = PN_SND_MIXED
    """The sender may send a mixture of settled and unsettled deliveries."""

    RCV_FIRST = PN_RCV_FIRST
    """The receiver will settle deliveries regardless of what the sender does."""
    RCV_SECOND = PN_RCV_SECOND
    """The receiver will only settle deliveries after the sender settles."""

    @staticmethod
    def wrap(impl):
        if isnull(impl):
            return None
        if pn_link_is_sender(impl):
            return Sender(impl)
        else:
            return Receiver(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_link_attachments)

    def _init(self) -> None:
        Endpoint._init(self)
        self.properties = None

    def _get_attachments(self):
        return pn_link_attachments(self._impl)

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, LinkException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_link_error(self._impl))))
        else:
            return err

    def _get_cond_impl(self):
        return pn_link_condition(self._impl)

    def _get_remote_cond_impl(self):
        return pn_link_remote_condition(self._impl)

    def open(self) -> None:
        """
        Opens the link.

        In more detail, this moves the local state of the link to the
        :const:`LOCAL_ACTIVE` state and triggers an attach frame to be
        sent to the peer. A link is fully active once both peers have
        attached it.
        """
        obj2dat(self.properties, pn_link_properties(self._impl))
        pn_link_open(self._impl)

    def close(self) -> None:
        """
        Closes the link.

        In more detail, this moves the local state of the link to the
        :const:`LOCAL_CLOSED` state and triggers an detach frame (with
        the closed flag set) to be sent to the peer. A link is fully
        closed once both peers have detached it.

        This may be called without calling :meth:`open`, in this case it
        is equivalent to calling :meth:`open` followed by :meth:`close`.
        """
        self._update_cond()
        pn_link_close(self._impl)

    @property
    def state(self) -> int:
        """
        The state of the link as a bit field. The state has a local
        and a remote component. Each of these can be in one of three
        states: ``UNINIT``, ``ACTIVE`` or ``CLOSED``. These can be
        tested by masking against :const:`LOCAL_UNINIT`,
        :const:`LOCAL_ACTIVE`, :const:`LOCAL_CLOSED`,
        :const:`REMOTE_UNINIT`, :const:`REMOTE_ACTIVE` and
        :const:`REMOTE_CLOSED`.
        """
        return pn_link_state(self._impl)

    @property
    def source(self) -> 'Terminus':
        """
        The source of the link as described by the local peer. The
        returned object is valid until the link is freed.
        """
        return Terminus(pn_link_source(self._impl))

    @property
    def target(self) -> 'Terminus':
        """
        The target of the link as described by the local peer. The
        returned object is valid until the link is freed.
        """
        return Terminus(pn_link_target(self._impl))

    @property
    def remote_source(self) -> 'Terminus':
        """
        The source of the link as described by the remote peer. The
        returned object is valid until the link is freed. The remote
        :class:`Terminus` object will be empty until the link is
        remotely opened as indicated by the :const:`REMOTE_ACTIVE`
        flag.
        """
        return Terminus(pn_link_remote_source(self._impl))

    @property
    def remote_target(self) -> 'Terminus':
        """
        The target of the link as described by the remote peer. The
        returned object is valid until the link is freed. The remote
        :class:`Terminus` object will be empty until the link is
        remotely opened as indicated by the :const:`REMOTE_ACTIVE`
        flag.
        """
        return Terminus(pn_link_remote_target(self._impl))

    @property
    def session(self) -> Session:
        """
        The parent session for this link.
        """
        return Session.wrap(pn_link_session(self._impl))

    @property
    def connection(self) -> Connection:
        """
        The connection on which this link was attached.
        """
        return self.session.connection

    @property
    def transport(self) -> Transport:
        """
        The transport bound to the connection on which this link was attached.
        """
        return self.session.transport

    def delivery(self, tag: str) -> Delivery:
        """
        Create a delivery. Every delivery object within a
        link must be supplied with a unique tag. Links
        maintain a sequence of delivery object in the order that
        they are created.

        :param tag: Delivery tag unique for this link.
        """
        return Delivery(pn_delivery(self._impl, tag))

    @property
    def current(self) -> Optional[Delivery]:
        """
        The current delivery for this link.

        Each link maintains a sequence of deliveries in the order
        they were created, along with a pointer to the *current*
        delivery. All send/recv operations on a link take place
        on the *current* delivery. If a link has no current delivery,
        the current delivery is automatically initialized to the
        next delivery created on the link. Once initialized, the
        current delivery remains the same until it is changed through
        use of :meth:`advance` or until it is settled via
        :meth:`Delivery.settle`.
        """
        return Delivery.wrap(pn_link_current(self._impl))

    def advance(self) -> bool:
        """
        Advance the current delivery of this link to the next delivery.

        For sending links this operation is used to finish sending message
        data for the current outgoing delivery and move on to the next
        outgoing delivery (if any).

        For receiving links, this operation is used to finish accessing
        message data from the current incoming delivery and move on to the
        next incoming delivery (if any).

        Each link maintains a sequence of deliveries in the order they were
        created, along with a pointer to the *current* delivery. The
        :meth:`advance` operation will modify the *current* delivery on the
        link to point to the next delivery in the sequence. If there is no
        next delivery in the sequence, the current delivery will be set to
        ``NULL``.

        :return: ``True`` if the value of the current delivery changed (even
            if it was set to ``NULL``, ``False`` otherwise.
        """
        return pn_link_advance(self._impl)

    @property
    def unsettled(self) -> int:
        """
        The number of unsettled deliveries for this link.
        """
        return pn_link_unsettled(self._impl)

    @property
    def credit(self) -> int:
        """
        The amount of outstanding credit on this link.

        Links use a credit based flow control scheme. Every receiver
        maintains a credit balance that corresponds to the number of
        deliveries that the receiver can accept at any given moment. As
        more capacity becomes available at the receiver (see
        :meth:`Receiver.flow`), it adds credit to this balance and
        communicates the new balance to the sender. Whenever a delivery
        is sent/received, the credit balance maintained by the link is
        decremented by one. Once the credit balance at the sender reaches
        zero, the sender must pause sending until more credit is obtained
        from the receiver.

        .. note:: A sending link may still be used to send deliveries even
            if :attr:`credit` reaches zero, however those deliveries will end
            up being buffered by the link until enough credit is obtained from
            the receiver to send them over the wire. In this case the balance
            reported by :attr:`credit` will go negative.
        """
        return pn_link_credit(self._impl)

    @property
    def available(self) -> int:
        """
        The available deliveries hint for this link.

        The available count for a link provides a hint as to the number of
        deliveries that might be able to be sent if sufficient credit were
        issued by the receiving link endpoint. See :meth:`Sender.offered` for
        more details.
        """
        return pn_link_available(self._impl)

    @property
    def queued(self) -> int:
        """
        The number of queued deliveries for a link.

        Links may queue deliveries for a number of reasons, for example
        there may be insufficient credit to send them to the receiver (see
        :meth:`credit`), or they simply may not have yet had a chance to
        be written to the wire. This operation will return the number of
        queued deliveries on a link.
        """
        return pn_link_queued(self._impl)

    def next(self, mask: int) -> Optional[Union['Sender', 'Receiver']]:
        """
        Retrieve the next link that matches the given state mask.

        When used with :meth:`Connection.link_head`, the application
        can access all links on the connection that match the given
        state. See :meth:`Connection.link_head` for a description of
        match behavior.

        :param mask: State mask to match
        :return: The next link that matches the given state mask, or
                 ``None`` if no link matches.
        """
        return Link.wrap(pn_link_next(self._impl, mask))

    @property
    def name(self) -> str:
        """
        The name of the link.
        """
        return pn_link_name(self._impl)

    @property
    def is_sender(self) -> bool:
        """
        ``True`` if this link is a sender, ``False`` otherwise.
        """
        return pn_link_is_sender(self._impl)

    @property
    def is_receiver(self) -> bool:
        """
        ``True`` if this link is a receiver, ``False`` otherwise.
        """
        return pn_link_is_receiver(self._impl)

    @property
    def remote_snd_settle_mode(self) -> int:
        """
        The remote sender settle mode for this link. One of
        :const:`SND_UNSETTLED`, :const:`SND_SETTLED` or
        :const:`SND_MIXED`.
        """
        return pn_link_remote_snd_settle_mode(self._impl)

    @property
    def remote_rcv_settle_mode(self) -> int:
        """
        The remote receiver settle mode for this link. One of
        :const:`RCV_FIRST` or :const:`RCV_SECOND`.
        """
        return pn_link_remote_rcv_settle_mode(self._impl)

    @property
    def snd_settle_mode(self) -> int:
        """The local sender settle mode for this link. One of
        :const:`SND_UNSETTLED`, :const:`SND_SETTLED` or
        :const:`SND_MIXED`.
        """
        return pn_link_snd_settle_mode(self._impl)

    @snd_settle_mode.setter
    def snd_settle_mode(self, mode: int) -> None:
        pn_link_set_snd_settle_mode(self._impl, mode)

    @property
    def rcv_settle_mode(self) -> int:
        """The local receiver settle mode for this link. One of
        :const:`RCV_FIRST` or :const:`RCV_SECOND`."""
        return pn_link_rcv_settle_mode(self._impl)

    @rcv_settle_mode.setter
    def rcv_settle_mode(self, mode: int) -> None:
        pn_link_set_rcv_settle_mode(self._impl, mode)

    @property
    def drain_mode(self) -> bool:
        """The drain mode on this link.

        If a link is in drain mode (``True``), then the sending
        endpoint of a link must immediately use up all available
        credit on the link. If this is not possible, the excess
        credit must be returned by invoking :meth:`drained`. Only
        the receiving endpoint can set the drain mode.

        When ``False``, this link is not in drain mode.
        """
        return pn_link_get_drain(self._impl)

    @drain_mode.setter
    def drain_mode(self, b: bool):
        pn_link_set_drain(self._impl, bool(b))

    def drained(self) -> int:
        """
        Drain excess credit for this link.

        When a link is in drain mode (see :attr:`drain_mode`), the
        sender must use all excess credit immediately, and release
        any excess credit back to the receiver if there are no
        deliveries available to send.

        When invoked on a sending link that is in drain mode, this
        operation will release all excess credit back to the receiver
        and return the number of credits released back to the sender.
        If the link is not in drain mode, this operation is a noop.

        When invoked on a receiving link, this operation will return
        and reset the number of credits the sender has released back
        to the receiver.

        :return: The number of credits drained.
        """
        return pn_link_drained(self._impl)

    @property
    def remote_max_message_size(self) -> int:
        """
        Get the remote view of the maximum message size for this link.

        .. warning:: **Unsettled API**

        A zero value means the size is unlimited.
        """
        return pn_link_remote_max_message_size(self._impl)

    @property
    def max_message_size(self) -> int:
        """The maximum message size for this link. A zero value means the
        size is unlimited.

        .. warning:: **Unsettled API**
        """
        return pn_link_max_message_size(self._impl)

    @max_message_size.setter
    def max_message_size(self, mode: int) -> None:
        pn_link_set_max_message_size(self._impl, mode)

    def detach(self) -> None:
        """
        Detach this link.
        """
        return pn_link_detach(self._impl)

    def free(self) -> None:
        """
        Free this link object. When a link object is freed,
        all :class:`Delivery` objects associated with the session (**<-- CHECK THIS**)
        are also freed. Freeing a link will settle any unsettled
        deliveries on the link.
        """
        pn_link_free(self._impl)

    @property
    def remote_properties(self):
        """
        The properties specified by the remote peer for this link.

        This operation will return a :class:`Data` object that
        is valid until the link object is freed. This :class:`Data`
        object will be empty until the remote link is opened as
        indicated by the :const:`REMOTE_ACTIVE` flag.

        :type: :class:`Data`
        """
        return dat2obj(pn_link_remote_properties(self._impl))

    @property
    def properties(self) -> Optional[PropertyDict]:
        """Link properties as a dictionary of key/values. The AMQP 1.0
        specification restricts this dictionary to have keys that are only
        :class:`symbol` types. It is possible to use the special ``dict``
        subclass :class:`PropertyDict` which will by default enforce this
        restrictions on construction. In addition, if strings type are used,
        this will silently convert them into symbols.
        """
        return self._properties_dict

    @properties.setter
    def properties(self, properties_dict: Optional[Dict['symbol', 'PythonAMQPData']]) -> None:
        if isinstance(properties_dict, dict):
            self._properties_dict = PropertyDict(properties_dict, raise_on_error=False)
        else:
            self._properties_dict = properties_dict


class Sender(Link):
    """
    A link over which messages are sent.
    """

    def offered(self, n: int) -> None:
        """
        Signal the availability of deliveries for this Sender.

        :param n: Credit the number of deliveries potentially
                  available for transfer.
        """
        pn_link_offered(self._impl, n)

    def stream(self, data: bytes) -> int:
        """
        Send specified data as part of the current delivery.

        :param data: Data to send
        """
        return self._check(pn_link_send(self._impl, data))

    def send(self, obj: Union[bytes, 'Message'], tag: Optional[str] = None) -> Union[int, Delivery]:
        """
        A convenience method to send objects as message content.

        Send specified object over this sender; the object is expected to
        have a ``send()`` method on it that takes the sender and an optional
        tag as arguments.

        Where the object is a :class:`Message`, this will send the message over
        this link, creating a new delivery for the purpose.
        """
        if hasattr(obj, 'send'):
            return obj.send(self, tag=tag)
        else:
            # treat object as bytes
            return self.stream(obj)

    def delivery_tag(self) -> str:
        """Increments and returns a counter to be used as the next message tag."""
        if not hasattr(self, 'tag_generator'):
            def simple_tags():
                count = 1
                while True:
                    yield str(count)
                    count += 1

            self.tag_generator = simple_tags()
        return next(self.tag_generator)


class Receiver(Link):
    """
    A link over which messages are received.
    """

    def flow(self, n: int) -> None:
        """
        Increases the credit issued to the remote sender by the specified number of messages.

        :param n: The credit to be issued to the remote sender.
        """
        pn_link_flow(self._impl, n)

    def recv(self, limit: int) -> Optional[bytes]:
        """
        Receive message data for the current delivery on this receiver.

        .. note:: The link API can be used to stream large messages across
            the network, so just because there is no data to read does not
            imply the message is complete. To ensure the entirety of the
            message data has been read, either invoke :meth:`recv` until
            ``None`` is returned.

        :param limit: the max data size to receive of this message
        :return: The received message data, or ``None`` if the message
            has been completely received.
        :raise: * :class:`Timeout` if timed out
                * :class:`Interrupt` if interrupted
                * :class:`LinkException` for all other exceptions
        """
        n, binary = pn_link_recv(self._impl, limit)
        if n == PN_EOS:
            return None
        else:
            self._check(n)
            return binary

    def drain(self, n: int) -> None:
        """
        Grant credit for incoming deliveries on this receiver, and
        set drain mode to true.

        Use :attr:`drain_mode` to set the drain mode explicitly.

        :param n: The amount by which to increment the link credit
        """
        pn_link_drain(self._impl, n)

    def draining(self) -> bool:
        """
        Check if a link is currently draining. A link is defined
        to be draining when drain mode is set to ``True``, and the
        sender still has excess credit.

        :return: ``True`` if the link is currently draining, ``False`` otherwise.
        """
        return pn_link_draining(self._impl)


class Terminus(object):
    """
    A source or target for messages.
    """
    UNSPECIFIED = PN_UNSPECIFIED
    """A nonexistent terminus, may used as a source or target."""
    SOURCE = PN_SOURCE
    """A source of messages."""
    TARGET = PN_TARGET
    """A target for messages."""
    COORDINATOR = PN_COORDINATOR
    """A special target identifying a transaction coordinator."""

    NONDURABLE = PN_NONDURABLE
    """A non durable terminus."""
    CONFIGURATION = PN_CONFIGURATION
    """A terminus with durably held configuration, but not delivery state."""
    DELIVERIES = PN_DELIVERIES
    """A terminus with both durably held configuration and durably held delivery state."""

    DIST_MODE_UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED
    """The behavior is defined by the node."""
    DIST_MODE_COPY = PN_DIST_MODE_COPY
    """The receiver gets all messages."""
    DIST_MODE_MOVE = PN_DIST_MODE_MOVE
    """The receiver competes for messages."""

    EXPIRE_WITH_LINK = PN_EXPIRE_WITH_LINK
    """The terminus is orphaned when the parent link is closed."""
    EXPIRE_WITH_SESSION = PN_EXPIRE_WITH_SESSION
    """The terminus is orphaned when the parent session is closed"""
    EXPIRE_WITH_CONNECTION = PN_EXPIRE_WITH_CONNECTION
    """The terminus is orphaned when the parent connection is closed"""
    EXPIRE_NEVER = PN_EXPIRE_NEVER
    """The terminus is never considered orphaned"""

    def __init__(self, impl):
        self._impl = impl

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, LinkException)
            raise exc("[%s]" % err)
        else:
            return err

    @property
    def type(self) -> int:
        """The terminus type, must be one of :const:`UNSPECIFIED`,
        :const:`SOURCE`, :const:`TARGET` or :const:`COORDINATOR`.
        """
        return pn_terminus_get_type(self._impl)

    @type.setter
    def type(self, type: int) -> None:
        self._check(pn_terminus_set_type(self._impl, type))

    @property
    def address(self) -> Optional[str]:
        """The address that identifies the source or target node"""
        return pn_terminus_get_address(self._impl)

    @address.setter
    def address(self, address: str) -> None:
        self._check(pn_terminus_set_address(self._impl, address))

    @property
    def durability(self) -> int:
        """The terminus durability mode, must be one of :const:`NONDURABLE`,
        :const:`CONFIGURATION` or :const:`DELIVERIES`.
        """
        return pn_terminus_get_durability(self._impl)

    @durability.setter
    def durability(self, mode: int):
        self._check(pn_terminus_set_durability(self._impl, mode))

    @property
    def expiry_policy(self) -> int:
        """The terminus expiry policy, must be one of :const:`EXPIRE_WITH_LINK`,
        :const:`EXPIRE_WITH_SESSION`, :const:`EXPIRE_WITH_CONNECTION` or
        :const:`EXPIRE_NEVER`.
        """
        return pn_terminus_get_expiry_policy(self._impl)

    @expiry_policy.setter
    def expiry_policy(self, policy: int):
        self._check(pn_terminus_set_expiry_policy(self._impl, policy))

    @property
    def timeout(self) -> int:
        """The terminus timeout in seconds."""
        return pn_terminus_get_timeout(self._impl)

    @timeout.setter
    def timeout(self, seconds: int) -> None:
        self._check(pn_terminus_set_timeout(self._impl, seconds))

    @property
    def dynamic(self) -> bool:
        """Indicates whether the source or target node was dynamically
        created"""
        return pn_terminus_is_dynamic(self._impl)

    @dynamic.setter
    def dynamic(self, dynamic: bool) -> None:
        self._check(pn_terminus_set_dynamic(self._impl, dynamic))

    @property
    def distribution_mode(self) -> int:
        """The terminus distribution mode, must be one of :const:`DIST_MODE_UNSPECIFIED`,
        :const:`DIST_MODE_COPY` or :const:`DIST_MODE_MOVE`.
        """
        return pn_terminus_get_distribution_mode(self._impl)

    @distribution_mode.setter
    def distribution_mode(self, mode: int) -> None:
        self._check(pn_terminus_set_distribution_mode(self._impl, mode))

    @property
    def properties(self):
        """
        Properties of a dynamic source or target.

        :type: :class:`Data` containing a map with :class:`symbol` keys.
        """
        return Data(pn_terminus_properties(self._impl))

    @property
    def capabilities(self):
        """
        Capabilities of the source or target.

        :type: :class:`Data` containing an array of :class:`symbol`.
        """
        return Data(pn_terminus_capabilities(self._impl))

    @property
    def outcomes(self):
        """
        Outcomes of the source or target.

        :type: :class:`Data` containing an array of :class:`symbol`.
       """
        return Data(pn_terminus_outcomes(self._impl))

    @property
    def filter(self):
        """
        A filter on a source allows the set of messages transferred over
        the link to be restricted. The symbol-keyed map represents a'
        filter set.

        :type: :class:`Data` containing a map with :class:`symbol` keys.
        """
        return Data(pn_terminus_filter(self._impl))

    def copy(self, src: 'Terminus') -> None:
        """
        Copy another terminus object.

        :param src: The terminus to be copied from
        :raises: :class:`LinkException` if there is an error
        """
        self._check(pn_terminus_copy(self._impl, src._impl))
