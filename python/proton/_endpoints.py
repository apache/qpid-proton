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

from __future__ import absolute_import

import weakref

from cproton import PN_LOCAL_UNINIT, PN_REMOTE_UNINIT, PN_LOCAL_ACTIVE, PN_REMOTE_ACTIVE, PN_LOCAL_CLOSED, \
    PN_REMOTE_CLOSED, \
    pn_object_reactor, pn_record_get_handler, pn_record_set_handler, pn_decref, \
    pn_connection, pn_connection_attachments, pn_connection_transport, pn_connection_error, pn_connection_condition, \
    pn_connection_remote_condition, pn_connection_collect, pn_connection_set_container, pn_connection_get_container, \
    pn_connection_get_hostname, pn_connection_set_hostname, pn_connection_get_user, pn_connection_set_user, \
    pn_connection_set_password, pn_connection_remote_container, pn_connection_remote_hostname, \
    pn_connection_remote_offered_capabilities, pn_connection_remote_desired_capabilities, \
    pn_connection_remote_properties, pn_connection_offered_capabilities, pn_connection_desired_capabilities, \
    pn_connection_properties, pn_connection_open, pn_connection_close, pn_connection_state, pn_connection_release, \
    pn_session, pn_session_head, pn_session_attachments, pn_session_condition, pn_session_remote_condition, \
    pn_session_get_incoming_capacity, pn_session_set_incoming_capacity, pn_session_get_outgoing_window, \
    pn_session_set_outgoing_window, pn_session_incoming_bytes, pn_session_outgoing_bytes, pn_session_open, \
    pn_session_close, pn_session_next, pn_session_state, pn_session_connection, pn_session_free, \
    PN_SND_UNSETTLED, PN_SND_SETTLED, PN_SND_MIXED, PN_RCV_FIRST, PN_RCV_SECOND, \
    pn_link_head, pn_link_is_sender, pn_link_attachments, pn_link_error, pn_link_condition, pn_link_remote_condition, \
    pn_link_open, pn_link_close, pn_link_state, pn_link_source, pn_link_target, pn_link_remote_source, \
    pn_link_remote_target, pn_link_session, pn_link_current, pn_link_advance, pn_link_unsettled, pn_link_credit, \
    pn_link_available, pn_link_queued, pn_link_next, pn_link_name, pn_link_is_receiver, pn_link_remote_snd_settle_mode, \
    pn_link_remote_rcv_settle_mode, pn_link_snd_settle_mode, pn_link_set_snd_settle_mode, pn_link_rcv_settle_mode, \
    pn_link_set_rcv_settle_mode, pn_link_get_drain, pn_link_set_drain, pn_link_drained, pn_link_remote_max_message_size, \
    pn_link_max_message_size, pn_link_set_max_message_size, pn_link_detach, pn_link_free, pn_link_offered, pn_link_send, \
    pn_link_flow, pn_link_recv, pn_link_drain, pn_link_draining, \
    pn_sender, pn_receiver, \
    PN_UNSPECIFIED, PN_SOURCE, PN_TARGET, PN_COORDINATOR, PN_NONDURABLE, PN_CONFIGURATION, \
    PN_DELIVERIES, PN_DIST_MODE_UNSPECIFIED, PN_DIST_MODE_COPY, PN_DIST_MODE_MOVE, PN_EXPIRE_WITH_LINK, \
    PN_EXPIRE_WITH_SESSION, PN_EXPIRE_WITH_CONNECTION, PN_EXPIRE_NEVER, \
    pn_terminus_set_durability, pn_terminus_set_timeout, pn_terminus_set_dynamic, pn_terminus_get_type, \
    pn_terminus_get_durability, pn_terminus_set_type, pn_terminus_get_address, pn_terminus_capabilities, \
    pn_terminus_set_address, pn_terminus_get_timeout, pn_terminus_filter, pn_terminus_properties, \
    pn_terminus_get_expiry_policy, pn_terminus_set_expiry_policy, pn_terminus_set_distribution_mode, \
    pn_terminus_get_distribution_mode, pn_terminus_copy, pn_terminus_outcomes, pn_terminus_is_dynamic, \
    PN_EOS, \
    pn_delivery, \
    pn_work_head, \
    pn_error_code, pn_error_text

from ._common import utf82unicode, unicode2utf8
from ._condition import obj2cond, cond2obj
from ._data import Data, obj2dat, dat2obj
from ._delivery import Delivery
from ._exceptions import EXCEPTIONS, LinkException, SessionException, ConnectionException
from ._transport import Transport
from ._wrapper import Wrapper


class Endpoint(object):
    LOCAL_UNINIT = PN_LOCAL_UNINIT
    REMOTE_UNINIT = PN_REMOTE_UNINIT
    LOCAL_ACTIVE = PN_LOCAL_ACTIVE
    REMOTE_ACTIVE = PN_REMOTE_ACTIVE
    LOCAL_CLOSED = PN_LOCAL_CLOSED
    REMOTE_CLOSED = PN_REMOTE_CLOSED

    def _init(self):
        self.condition = None

    def _update_cond(self):
        obj2cond(self.condition, self._get_cond_impl())

    @property
    def remote_condition(self):
        return cond2obj(self._get_remote_cond_impl())

    # the following must be provided by subclasses
    def _get_cond_impl(self):
        assert False, "Subclass must override this!"

    def _get_remote_cond_impl(self):
        assert False, "Subclass must override this!"

    def _get_handler(self):
        from . import _reactor
        from . import _reactor_impl
        ractor = _reactor.Reactor.wrap(pn_object_reactor(self._impl))
        if ractor:
            on_error = ractor.on_error_delegate()
        else:
            on_error = None
        record = self._get_attachments()
        return _reactor_impl.WrappedHandler.wrap(pn_record_get_handler(record), on_error)

    def _set_handler(self, handler):
        from . import _reactor
        from . import _reactor_impl
        ractor = _reactor.Reactor.wrap(pn_object_reactor(self._impl))
        if ractor:
            on_error = ractor.on_error_delegate()
        else:
            on_error = None
        impl = _reactor_impl._chandler(handler, on_error)
        record = self._get_attachments()
        pn_record_set_handler(record, impl)
        pn_decref(impl)

    handler = property(_get_handler, _set_handler)

    @property
    def transport(self):
        return self.connection.transport


class Connection(Wrapper, Endpoint):
    """
    A representation of an AMQP connection
    """

    @staticmethod
    def wrap(impl):
        if impl is None:
            return None
        else:
            return Connection(impl)

    def __init__(self, impl=pn_connection):
        Wrapper.__init__(self, impl, pn_connection_attachments)

    def _init(self):
        Endpoint._init(self)
        self.offered_capabilities = None
        self.desired_capabilities = None
        self.properties = None

    def _get_attachments(self):
        return pn_connection_attachments(self._impl)

    @property
    def connection(self):
        return self

    @property
    def transport(self):
        return Transport.wrap(pn_connection_transport(self._impl))

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, ConnectionException)
            raise exc("[%s]: %s" % (err, pn_connection_error(self._impl)))
        else:
            return err

    def _get_cond_impl(self):
        return pn_connection_condition(self._impl)

    def _get_remote_cond_impl(self):
        return pn_connection_remote_condition(self._impl)

    def collect(self, collector):
        if collector is None:
            pn_connection_collect(self._impl, None)
        else:
            pn_connection_collect(self._impl, collector._impl)
        self._collector = weakref.ref(collector)

    def _get_container(self):
        return utf82unicode(pn_connection_get_container(self._impl))

    def _set_container(self, name):
        return pn_connection_set_container(self._impl, unicode2utf8(name))

    container = property(_get_container, _set_container)

    def _get_hostname(self):
        return utf82unicode(pn_connection_get_hostname(self._impl))

    def _set_hostname(self, name):
        return pn_connection_set_hostname(self._impl, unicode2utf8(name))

    hostname = property(_get_hostname, _set_hostname,
                        doc="""
Set the name of the host (either fully qualified or relative) to which this
connection is connecting to.  This information may be used by the remote
peer to determine the correct back-end service to connect the client to.
This value will be sent in the Open performative, and will be used by SSL
and SASL layers to identify the peer.
""")

    def _get_user(self):
        return utf82unicode(pn_connection_get_user(self._impl))

    def _set_user(self, name):
        return pn_connection_set_user(self._impl, unicode2utf8(name))

    user = property(_get_user, _set_user)

    def _get_password(self):
        return None

    def _set_password(self, name):
        return pn_connection_set_password(self._impl, unicode2utf8(name))

    password = property(_get_password, _set_password)

    @property
    def remote_container(self):
        """The container identifier specified by the remote peer for this connection."""
        return pn_connection_remote_container(self._impl)

    @property
    def remote_hostname(self):
        """The hostname specified by the remote peer for this connection."""
        return pn_connection_remote_hostname(self._impl)

    @property
    def remote_offered_capabilities(self):
        """The capabilities offered by the remote peer for this connection."""
        return dat2obj(pn_connection_remote_offered_capabilities(self._impl))

    @property
    def remote_desired_capabilities(self):
        """The capabilities desired by the remote peer for this connection."""
        return dat2obj(pn_connection_remote_desired_capabilities(self._impl))

    @property
    def remote_properties(self):
        """The properties specified by the remote peer for this connection."""
        return dat2obj(pn_connection_remote_properties(self._impl))

    def open(self):
        """
        Opens the connection.

        In more detail, this moves the local state of the connection to
        the ACTIVE state and triggers an open frame to be sent to the
        peer. A connection is fully active once both peers have opened it.
        """
        obj2dat(self.offered_capabilities,
                pn_connection_offered_capabilities(self._impl))
        obj2dat(self.desired_capabilities,
                pn_connection_desired_capabilities(self._impl))
        obj2dat(self.properties, pn_connection_properties(self._impl))
        pn_connection_open(self._impl)

    def close(self):
        """
        Closes the connection.

        In more detail, this moves the local state of the connection to
        the CLOSED state and triggers a close frame to be sent to the
        peer. A connection is fully closed once both peers have closed it.
        """
        self._update_cond()
        pn_connection_close(self._impl)
        if hasattr(self, '_session_policy'):
            # break circular ref
            del self._session_policy

    @property
    def state(self):
        """
        The state of the connection as a bit field. The state has a local
        and a remote component. Each of these can be in one of three
        states: UNINIT, ACTIVE or CLOSED. These can be tested by masking
        against LOCAL_UNINIT, LOCAL_ACTIVE, LOCAL_CLOSED, REMOTE_UNINIT,
        REMOTE_ACTIVE and REMOTE_CLOSED.
        """
        return pn_connection_state(self._impl)

    def session(self):
        """
        Returns a new session on this connection.
        """
        ssn = pn_session(self._impl)
        if ssn is None:
            raise (SessionException("Session allocation failed."))
        else:
            return Session(ssn)

    def session_head(self, mask):
        return Session.wrap(pn_session_head(self._impl, mask))

    def link_head(self, mask):
        return Link.wrap(pn_link_head(self._impl, mask))

    @property
    def work_head(self):
        return Delivery.wrap(pn_work_head(self._impl))

    @property
    def error(self):
        return pn_error_code(pn_connection_error(self._impl))

    def free(self):
        pn_connection_release(self._impl)


class Session(Wrapper, Endpoint):

    @staticmethod
    def wrap(impl):
        if impl is None:
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

    def _get_incoming_capacity(self):
        return pn_session_get_incoming_capacity(self._impl)

    def _set_incoming_capacity(self, capacity):
        pn_session_set_incoming_capacity(self._impl, capacity)

    incoming_capacity = property(_get_incoming_capacity, _set_incoming_capacity)

    def _get_outgoing_window(self):
        return pn_session_get_outgoing_window(self._impl)

    def _set_outgoing_window(self, window):
        pn_session_set_outgoing_window(self._impl, window)

    outgoing_window = property(_get_outgoing_window, _set_outgoing_window)

    @property
    def outgoing_bytes(self):
        return pn_session_outgoing_bytes(self._impl)

    @property
    def incoming_bytes(self):
        return pn_session_incoming_bytes(self._impl)

    def open(self):
        pn_session_open(self._impl)

    def close(self):
        self._update_cond()
        pn_session_close(self._impl)

    def next(self, mask):
        return Session.wrap(pn_session_next(self._impl, mask))

    @property
    def state(self):
        return pn_session_state(self._impl)

    @property
    def connection(self):
        return Connection.wrap(pn_session_connection(self._impl))

    def sender(self, name):
        return Sender(pn_sender(self._impl, unicode2utf8(name)))

    def receiver(self, name):
        return Receiver(pn_receiver(self._impl, unicode2utf8(name)))

    def free(self):
        pn_session_free(self._impl)


class Link(Wrapper, Endpoint):
    """
    A representation of an AMQP link, of which there are two concrete
    implementations, Sender and Receiver.
    """

    SND_UNSETTLED = PN_SND_UNSETTLED
    SND_SETTLED = PN_SND_SETTLED
    SND_MIXED = PN_SND_MIXED

    RCV_FIRST = PN_RCV_FIRST
    RCV_SECOND = PN_RCV_SECOND

    @staticmethod
    def wrap(impl):
        if impl is None: return None
        if pn_link_is_sender(impl):
            return Sender(impl)
        else:
            return Receiver(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_link_attachments)

    def _get_attachments(self):
        return pn_link_attachments(self._impl)

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, LinkException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_link_error(self._impl))))
        else:
            return err

    def _get_cond_impl(self):
        return pn_link_condition(self._impl)

    def _get_remote_cond_impl(self):
        return pn_link_remote_condition(self._impl)

    def open(self):
        """
        Opens the link.

        In more detail, this moves the local state of the link to the
        ACTIVE state and triggers an attach frame to be sent to the
        peer. A link is fully active once both peers have attached it.
        """
        pn_link_open(self._impl)

    def close(self):
        """
        Closes the link.

        In more detail, this moves the local state of the link to the
        CLOSED state and triggers an detach frame (with the closed flag
        set) to be sent to the peer. A link is fully closed once both
        peers have detached it.
        """
        self._update_cond()
        pn_link_close(self._impl)

    @property
    def state(self):
        """
        The state of the link as a bit field. The state has a local
        and a remote component. Each of these can be in one of three
        states: UNINIT, ACTIVE or CLOSED. These can be tested by masking
        against LOCAL_UNINIT, LOCAL_ACTIVE, LOCAL_CLOSED, REMOTE_UNINIT,
        REMOTE_ACTIVE and REMOTE_CLOSED.
        """
        return pn_link_state(self._impl)

    @property
    def source(self):
        """The source of the link as described by the local peer."""
        return Terminus(pn_link_source(self._impl))

    @property
    def target(self):
        """The target of the link as described by the local peer."""
        return Terminus(pn_link_target(self._impl))

    @property
    def remote_source(self):
        """The source of the link as described by the remote peer."""
        return Terminus(pn_link_remote_source(self._impl))

    @property
    def remote_target(self):
        """The target of the link as described by the remote peer."""
        return Terminus(pn_link_remote_target(self._impl))

    @property
    def session(self):
        return Session.wrap(pn_link_session(self._impl))

    @property
    def connection(self):
        """The connection on which this link was attached."""
        return self.session.connection

    def delivery(self, tag):
        return Delivery(pn_delivery(self._impl, tag))

    @property
    def current(self):
        return Delivery.wrap(pn_link_current(self._impl))

    def advance(self):
        return pn_link_advance(self._impl)

    @property
    def unsettled(self):
        return pn_link_unsettled(self._impl)

    @property
    def credit(self):
        """The amount of outstanding credit on this link."""
        return pn_link_credit(self._impl)

    @property
    def available(self):
        return pn_link_available(self._impl)

    @property
    def queued(self):
        return pn_link_queued(self._impl)

    def next(self, mask):
        return Link.wrap(pn_link_next(self._impl, mask))

    @property
    def name(self):
        """Returns the name of the link"""
        return utf82unicode(pn_link_name(self._impl))

    @property
    def is_sender(self):
        """Returns true if this link is a sender."""
        return pn_link_is_sender(self._impl)

    @property
    def is_receiver(self):
        """Returns true if this link is a receiver."""
        return pn_link_is_receiver(self._impl)

    @property
    def remote_snd_settle_mode(self):
        return pn_link_remote_snd_settle_mode(self._impl)

    @property
    def remote_rcv_settle_mode(self):
        return pn_link_remote_rcv_settle_mode(self._impl)

    def _get_snd_settle_mode(self):
        return pn_link_snd_settle_mode(self._impl)

    def _set_snd_settle_mode(self, mode):
        pn_link_set_snd_settle_mode(self._impl, mode)

    snd_settle_mode = property(_get_snd_settle_mode, _set_snd_settle_mode)

    def _get_rcv_settle_mode(self):
        return pn_link_rcv_settle_mode(self._impl)

    def _set_rcv_settle_mode(self, mode):
        pn_link_set_rcv_settle_mode(self._impl, mode)

    rcv_settle_mode = property(_get_rcv_settle_mode, _set_rcv_settle_mode)

    def _get_drain(self):
        return pn_link_get_drain(self._impl)

    def _set_drain(self, b):
        pn_link_set_drain(self._impl, bool(b))

    drain_mode = property(_get_drain, _set_drain)

    def drained(self):
        return pn_link_drained(self._impl)

    @property
    def remote_max_message_size(self):
        return pn_link_remote_max_message_size(self._impl)

    def _get_max_message_size(self):
        return pn_link_max_message_size(self._impl)

    def _set_max_message_size(self, mode):
        pn_link_set_max_message_size(self._impl, mode)

    max_message_size = property(_get_max_message_size, _set_max_message_size)

    def detach(self):
        return pn_link_detach(self._impl)

    def free(self):
        pn_link_free(self._impl)


class Sender(Link):
    """
    A link over which messages are sent.
    """

    def offered(self, n):
        pn_link_offered(self._impl, n)

    def stream(self, data):
        """
        Send specified data as part of the current delivery

        @type data: binary
        @param data: data to send
        """
        return self._check(pn_link_send(self._impl, data))

    def send(self, obj, tag=None):
        """
        Send specified object over this sender; the object is expected to
        have a send() method on it that takes the sender and an optional
        tag as arguments.

        Where the object is a Message, this will send the message over
        this link, creating a new delivery for the purpose.
        """
        if hasattr(obj, 'send'):
            return obj.send(self, tag=tag)
        else:
            # treat object as bytes
            return self.stream(obj)

    def delivery_tag(self):
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

    def flow(self, n):
        """Increases the credit issued to the remote sender by the specified number of messages."""
        pn_link_flow(self._impl, n)

    def recv(self, limit):
        n, binary = pn_link_recv(self._impl, limit)
        if n == PN_EOS:
            return None
        else:
            self._check(n)
            return binary

    def drain(self, n):
        pn_link_drain(self._impl, n)

    def draining(self):
        return pn_link_draining(self._impl)


class Terminus(object):
    UNSPECIFIED = PN_UNSPECIFIED
    SOURCE = PN_SOURCE
    TARGET = PN_TARGET
    COORDINATOR = PN_COORDINATOR

    NONDURABLE = PN_NONDURABLE
    CONFIGURATION = PN_CONFIGURATION
    DELIVERIES = PN_DELIVERIES

    DIST_MODE_UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED
    DIST_MODE_COPY = PN_DIST_MODE_COPY
    DIST_MODE_MOVE = PN_DIST_MODE_MOVE

    EXPIRE_WITH_LINK = PN_EXPIRE_WITH_LINK
    EXPIRE_WITH_SESSION = PN_EXPIRE_WITH_SESSION
    EXPIRE_WITH_CONNECTION = PN_EXPIRE_WITH_CONNECTION
    EXPIRE_NEVER = PN_EXPIRE_NEVER

    def __init__(self, impl):
        self._impl = impl

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, LinkException)
            raise exc("[%s]" % err)
        else:
            return err

    def _get_type(self):
        return pn_terminus_get_type(self._impl)

    def _set_type(self, type):
        self._check(pn_terminus_set_type(self._impl, type))

    type = property(_get_type, _set_type)

    def _get_address(self):
        """The address that identifies the source or target node"""
        return utf82unicode(pn_terminus_get_address(self._impl))

    def _set_address(self, address):
        self._check(pn_terminus_set_address(self._impl, unicode2utf8(address)))

    address = property(_get_address, _set_address)

    def _get_durability(self):
        return pn_terminus_get_durability(self._impl)

    def _set_durability(self, seconds):
        self._check(pn_terminus_set_durability(self._impl, seconds))

    durability = property(_get_durability, _set_durability)

    def _get_expiry_policy(self):
        return pn_terminus_get_expiry_policy(self._impl)

    def _set_expiry_policy(self, seconds):
        self._check(pn_terminus_set_expiry_policy(self._impl, seconds))

    expiry_policy = property(_get_expiry_policy, _set_expiry_policy)

    def _get_timeout(self):
        return pn_terminus_get_timeout(self._impl)

    def _set_timeout(self, seconds):
        self._check(pn_terminus_set_timeout(self._impl, seconds))

    timeout = property(_get_timeout, _set_timeout)

    def _is_dynamic(self):
        """Indicates whether the source or target node was dynamically
        created"""
        return pn_terminus_is_dynamic(self._impl)

    def _set_dynamic(self, dynamic):
        self._check(pn_terminus_set_dynamic(self._impl, dynamic))

    dynamic = property(_is_dynamic, _set_dynamic)

    def _get_distribution_mode(self):
        return pn_terminus_get_distribution_mode(self._impl)

    def _set_distribution_mode(self, mode):
        self._check(pn_terminus_set_distribution_mode(self._impl, mode))

    distribution_mode = property(_get_distribution_mode, _set_distribution_mode)

    @property
    def properties(self):
        """Properties of a dynamic source or target."""
        return Data(pn_terminus_properties(self._impl))

    @property
    def capabilities(self):
        """Capabilities of the source or target."""
        return Data(pn_terminus_capabilities(self._impl))

    @property
    def outcomes(self):
        return Data(pn_terminus_outcomes(self._impl))

    @property
    def filter(self):
        """A filter on a source allows the set of messages transfered over
        the link to be restricted"""
        return Data(pn_terminus_filter(self._impl))

    def copy(self, src):
        self._check(pn_terminus_copy(self._impl, src._impl))
