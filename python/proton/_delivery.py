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

from cproton import PN_ACCEPTED, PN_MODIFIED, PN_RECEIVED, PN_REJECTED, PN_RELEASED, pn_delivery_abort, \
    pn_delivery_aborted, pn_delivery_attachments, pn_delivery_link, pn_delivery_local, pn_delivery_local_state, \
    pn_delivery_partial, pn_delivery_pending, pn_delivery_readable, pn_delivery_remote, pn_delivery_remote_state, \
    pn_delivery_settle, pn_delivery_settled, pn_delivery_tag, pn_delivery_update, pn_delivery_updated, \
    pn_delivery_writable, pn_disposition_annotations, pn_disposition_condition, pn_disposition_data, \
    pn_disposition_get_section_number, pn_disposition_get_section_offset, pn_disposition_is_failed, \
    pn_disposition_is_undeliverable, pn_disposition_set_failed, pn_disposition_set_section_number, \
    pn_disposition_set_section_offset, pn_disposition_set_undeliverable, pn_disposition_type, \
    isnull

from ._condition import cond2obj, obj2cond
from ._data import dat2obj, obj2dat
from ._wrapper import Wrapper

from typing import Dict, List, Optional, Type, Union, TYPE_CHECKING, Any

if TYPE_CHECKING:
    from ._condition import Condition
    from ._data import PythonAMQPData, symbol
    from ._endpoints import Receiver, Sender  # circular import
    from ._reactor import Connection, Session, Transport


class NamedInt(int):
    values: Dict[int, 'DispositionType'] = {}

    def __new__(cls: Type['DispositionType'], i: int, name: str) -> 'DispositionType':
        ni = super(NamedInt, cls).__new__(cls, i)
        cls.values[i] = ni
        return ni

    def __init__(self, i: int, name: str) -> None:
        self.name = name

    def __repr__(self) -> str:
        return self.name

    def __str__(self) -> str:
        return self.name

    @classmethod
    def get(cls, i: int) -> Union[int, 'DispositionType']:
        return cls.values.get(i, i)


class DispositionType(NamedInt):
    values = {}


class Disposition(object):
    """
    A delivery state.

    Dispositions record the current state or final outcome of a
    transfer. Every delivery contains both a local and remote
    disposition. The local disposition holds the local state of the
    delivery, and the remote disposition holds the last known remote
    state of the delivery.
    """

    RECEIVED = DispositionType(PN_RECEIVED, "RECEIVED")
    """
    A non terminal state indicating how much (if any) message data
    has been received for a delivery.
    """

    ACCEPTED = DispositionType(PN_ACCEPTED, "ACCEPTED")
    """
    A terminal state indicating that the delivery was successfully
    processed. Once in this state there will be no further state
    changes prior to the delivery being settled.
    """

    REJECTED = DispositionType(PN_REJECTED, "REJECTED")
    """
    A terminal state indicating that the delivery could not be
    processed due to some error condition. Once in this state
    there will be no further state changes prior to the delivery
    being settled.
    """

    RELEASED = DispositionType(PN_RELEASED, "RELEASED")
    """
    A terminal state indicating that the delivery is being
    returned to the sender. Once in this state there will be no
    further state changes prior to the delivery being settled.
    """

    MODIFIED = DispositionType(PN_MODIFIED, "MODIFIED")
    """
    A terminal state indicating that the delivery is being
    returned to the sender and should be annotated by the
    sender prior to further delivery attempts. Once in this
    state there will be no further state changes prior to the
    delivery being settled.
    """

    def __init__(self, impl, local):
        self._impl = impl
        self.local = local
        self._data = None
        self._condition = None
        self._annotations = None

    @property
    def type(self) -> Union[int, DispositionType]:
        """
        Get the type of this disposition object.

        Defined values are:

        * :const:`RECEIVED`
        * :const:`ACCEPTED`
        * :const:`REJECTED`
        * :const:`RELEASED`
        * :const:`MODIFIED`
        """
        return DispositionType.get(pn_disposition_type(self._impl))

    @property
    def section_number(self) -> int:
        """The section number associated with a disposition."""
        return pn_disposition_get_section_number(self._impl)

    @section_number.setter
    def section_number(self, n: int) -> None:
        pn_disposition_set_section_number(self._impl, n)

    @property
    def section_offset(self) -> int:
        """The section offset associated with a disposition."""
        return pn_disposition_get_section_offset(self._impl)

    @section_offset.setter
    def section_offset(self, n: int) -> None:
        pn_disposition_set_section_offset(self._impl, n)

    @property
    def failed(self) -> bool:
        """The failed flag for this disposition."""
        return pn_disposition_is_failed(self._impl)

    @failed.setter
    def failed(self, b: bool) -> None:
        pn_disposition_set_failed(self._impl, b)

    @property
    def undeliverable(self) -> bool:
        """The undeliverable flag for this disposition."""
        return pn_disposition_is_undeliverable(self._impl)

    @undeliverable.setter
    def undeliverable(self, b: bool) -> None:
        pn_disposition_set_undeliverable(self._impl, b)

    @property
    def data(self) -> Optional[List[int]]:
        """Access the disposition as a :class:`Data` object.

        Dispositions are an extension point in the AMQP protocol. The
        disposition interface provides setters/getters for those
        dispositions that are predefined by the specification, however
        access to the raw disposition data is provided so that other
        dispositions can be used.

        The :class:`Data` object returned by this operation is valid until
        the parent delivery is settled.
        """
        if self.local:
            return self._data
        else:
            return dat2obj(pn_disposition_data(self._impl))

    @data.setter
    def data(self, obj: List[int]) -> None:
        if self.local:
            self._data = obj
        else:
            raise AttributeError("data attribute is read-only")

    @property
    def annotations(self) -> Optional[Dict['symbol', 'PythonAMQPData']]:
        """The annotations associated with a disposition.

        The :class:`Data` object retrieved by this operation may be modified
        prior to updating a delivery. When a delivery is updated, the
        annotations described by the :class:`Data` are reported to the peer
        if applicable to the current delivery state, e.g. states such as
        :const:`MODIFIED`. The :class:`Data` must be empty or contain a symbol
        keyed map.

        The :class:`Data` object returned by this operation is valid until
        the parent delivery is settled.
        """
        if self.local:
            return self._annotations
        else:
            return dat2obj(pn_disposition_annotations(self._impl))

    @annotations.setter
    def annotations(self, obj: Dict[str, 'PythonAMQPData']) -> None:
        if self.local:
            self._annotations = obj
        else:
            raise AttributeError("annotations attribute is read-only")

    @property
    def condition(self) -> Optional['Condition']:
        """The condition object associated with a disposition.

        The :class:`Condition` object retrieved by this operation may be
        modified prior to updating a delivery. When a delivery is updated,
        the condition described by the disposition is reported to the peer
        if applicable to the current delivery state, e.g. states such as
        :const:`REJECTED`.
        """
        if self.local:
            return self._condition
        else:
            return cond2obj(pn_disposition_condition(self._impl))

    @condition.setter
    def condition(self, obj: 'Condition') -> None:
        if self.local:
            self._condition = obj
        else:
            raise AttributeError("condition attribute is read-only")


class Delivery(Wrapper):
    """
    Tracks and/or records the delivery of a message over a link.
    """

    RECEIVED = Disposition.RECEIVED
    """
    A non terminal state indicating how much (if any) message data
    has been received for a delivery.
    """

    ACCEPTED = Disposition.ACCEPTED
    """
    A terminal state indicating that the delivery was successfully
    processed. Once in this state there will be no further state
    changes prior to the delivery being settled.
    """

    REJECTED = Disposition.REJECTED
    """
    A terminal state indicating that the delivery could not be
    processed due to some error condition. Once in this state
    there will be no further state changes prior to the delivery
    being settled.
    """

    RELEASED = Disposition.RELEASED
    """
    A terminal state indicating that the delivery is being
    returned to the sender. Once in this state there will be no
    further state changes prior to the delivery being settled.
    """

    MODIFIED = Disposition.MODIFIED
    """
    A terminal state indicating that the delivery is being
    returned to the sender and should be annotated by the
    sender prior to further delivery attempts. Once in this
    state there will be no further state changes prior to the
    delivery being settled.
    """

    @staticmethod
    def wrap(impl):
        if isnull(impl):
            return None
        else:
            return Delivery(impl)

    def __init__(self, impl):
        Wrapper.__init__(self, impl, pn_delivery_attachments)

    def _init(self) -> None:
        self.local = Disposition(pn_delivery_local(self._impl), True)
        self.remote = Disposition(pn_delivery_remote(self._impl), False)

    @property
    def tag(self) -> str:
        """
        The identifier for the delivery.
        """
        return pn_delivery_tag(self._impl)

    @property
    def writable(self) -> bool:
        """
        ``True`` for an outgoing delivery to which data can now be written,
        ``False`` otherwise..
        """
        return pn_delivery_writable(self._impl)

    @property
    def readable(self) -> bool:
        """
        ``True`` for an incoming delivery that has data to read,
        ``False`` otherwise..
        """
        return pn_delivery_readable(self._impl)

    @property
    def updated(self) -> bool:
        """
        ``True`` if the state of the delivery has been updated
        (e.g. it has been settled and/or accepted, rejected etc),
        ``False`` otherwise.
        """
        return pn_delivery_updated(self._impl)

    def update(self, state: Union[int, DispositionType]) -> None:
        """
        Set the local state of the delivery e.g. :const:`ACCEPTED`,
        :const:`REJECTED`, :const:`RELEASED`.

        :param state: State of delivery
        """
        obj2dat(self.local._data, pn_disposition_data(self.local._impl))
        obj2dat(self.local._annotations, pn_disposition_annotations(self.local._impl))
        obj2cond(self.local._condition, pn_disposition_condition(self.local._impl))
        pn_delivery_update(self._impl, state)

    @property
    def pending(self) -> int:
        """
        The amount of pending message data for a delivery.
        """
        return pn_delivery_pending(self._impl)

    @property
    def partial(self) -> bool:
        """
        ``True`` for an incoming delivery if not all the data is
        yet available, ``False`` otherwise.
        """
        return pn_delivery_partial(self._impl)

    @property
    def local_state(self) -> DispositionType:
        """A local state of the delivery."""
        return DispositionType.get(pn_delivery_local_state(self._impl))

    @property
    def remote_state(self) -> Union[int, DispositionType]:
        """A remote state of the delivery as indicated by the remote peer."""
        return DispositionType.get(pn_delivery_remote_state(self._impl))

    @property
    def settled(self) -> bool:
        """
        ``True`` if the delivery has been settled by the remote peer,
        ``False`` otherwise.
        """
        return pn_delivery_settled(self._impl)

    def settle(self) -> None:
        """
        Settles the delivery locally. This indicates the application
        considers the delivery complete and does not wish to receive any
        further events about it. Every delivery should be settled locally.
        """
        pn_delivery_settle(self._impl)

    @property
    def aborted(self) -> bool:
        """
        ``True`` if the delivery has been aborted, ``False`` otherwise.
        """
        return pn_delivery_aborted(self._impl)

    def abort(self) -> None:
        """
        Aborts the delivery.  This indicates the application wishes to
        invalidate any data that may have already been sent on this delivery.
        The delivery cannot be aborted after it has been completely delivered.
        """
        pn_delivery_abort(self._impl)

    @property
    def link(self) -> Union['Receiver', 'Sender']:
        """
        The :class:`Link` on which the delivery was sent or received.
        """
        from . import _endpoints
        return _endpoints.Link.wrap(pn_delivery_link(self._impl))

    @property
    def session(self) -> 'Session':
        """
        The :class:`Session` over which the delivery was sent or received.
        """
        return self.link.session

    @property
    def connection(self) -> 'Connection':
        """
        The :class:`Connection` over which the delivery was sent or received.
        """
        return self.session.connection

    @property
    def transport(self) -> 'Transport':
        """
        The :class:`Transport` bound to the :class:`Connection` over which
        the delivery was sent or received.
        """
        return self.connection.transport
