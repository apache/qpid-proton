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

from cproton import (PN_ACCEPTED, PN_MODIFIED, PN_RECEIVED, PN_REJECTED, PN_RELEASED, PN_TRANSACTIONAL_STATE,
                     pn_delivery_abort, pn_delivery_aborted, pn_delivery_attachments, pn_delivery_link,
                     pn_delivery_local, pn_delivery_local_state,
                     pn_delivery_partial, pn_delivery_pending, pn_delivery_readable, pn_delivery_remote,
                     pn_delivery_remote_state,
                     pn_delivery_settle, pn_delivery_settled, pn_delivery_tag, pn_delivery_update, pn_delivery_updated,
                     pn_delivery_writable, pn_disposition_annotations, pn_disposition_condition, pn_disposition_data,
                     pn_disposition_get_section_number, pn_disposition_get_section_offset, pn_disposition_is_failed,
                     pn_disposition_is_undeliverable, pn_disposition_set_failed, pn_disposition_set_section_number,
                     pn_disposition_set_section_offset, pn_disposition_set_undeliverable, pn_disposition_type,
                     pn_custom_disposition,
                     pn_custom_disposition_get_type,
                     pn_custom_disposition_set_type,
                     pn_custom_disposition_data,
                     pn_rejected_disposition,
                     pn_rejected_disposition_condition,
                     pn_received_disposition,
                     pn_received_disposition_get_section_number,
                     pn_received_disposition_set_section_number,
                     pn_received_disposition_get_section_offset,
                     pn_received_disposition_set_section_offset,
                     pn_modified_disposition,
                     pn_modified_disposition_is_failed,
                     pn_modified_disposition_set_failed,
                     pn_modified_disposition_is_undeliverable,
                     pn_modified_disposition_set_undeliverable,
                     pn_modified_disposition_annotations,
                     pn_transactional_disposition,
                     pn_transactional_disposition_get_id,
                     pn_transactional_disposition_set_id,
                     pn_transactional_disposition_get_outcome_type,
                     pn_transactional_disposition_set_outcome_type,
                     pn_unsettled_next)

from ._condition import cond2obj, obj2cond, Condition
from ._data import dat2obj, obj2dat
from ._transport import Transport
from ._wrapper import Wrapper

from enum import IntEnum
from typing import Any, Optional, Union, TYPE_CHECKING


if TYPE_CHECKING:
    from ._data import PythonAMQPData, symbol
    from ._endpoints import Receiver, Sender  # circular import
    from ._reactor import Connection, Session


class DispositionType(IntEnum):
    RECEIVED = PN_RECEIVED
    """
    A non terminal state indicating how much (if any) message data
    has been received for a delivery.
    """

    ACCEPTED = PN_ACCEPTED
    """
    A terminal state indicating that the delivery was successfully
    processed. Once in this state there will be no further state
    changes prior to the delivery being settled.
    """

    REJECTED = PN_REJECTED
    """
    A terminal state indicating that the delivery could not be
    processed due to some error condition. Once in this state
    there will be no further state changes prior to the delivery
    being settled.
    """

    RELEASED = PN_RELEASED
    """
    A terminal state indicating that the delivery is being
    returned to the sender. Once in this state there will be no
    further state changes prior to the delivery being settled.
    """

    MODIFIED = PN_MODIFIED
    """
    A terminal state indicating that the delivery is being
    returned to the sender and should be annotated by the
    sender prior to further delivery attempts. Once in this
    state there will be no further state changes prior to the
    delivery being settled.
    """

    TRANSACTIONAL_STATE = PN_TRANSACTIONAL_STATE
    """
    A non-terminal delivery state indicating the transactional
    state of a delivery
    """

    @classmethod
    def or_int(cls, i: int) -> Union[int, 'DispositionType']:
        return cls(i) if i in cls._value2member_map_ else i


class Disposition:
    """
    A delivery state.

    Dispositions record the current state or final outcome of a
    transfer. Every delivery contains both a local and remote
    disposition. The local disposition holds the local state of the
    delivery, and the remote disposition holds the last known remote
    state of the delivery.
    """

    RECEIVED = DispositionType.RECEIVED
    ACCEPTED = DispositionType.ACCEPTED
    REJECTED = DispositionType.REJECTED
    RELEASED = DispositionType.RELEASED
    MODIFIED = DispositionType.MODIFIED
    TRANSACTIONAL_STATE = DispositionType.TRANSACTIONAL_STATE


class RemoteDisposition(Disposition):

    def __new__(cls, delivery_impl):
        state = DispositionType.or_int(pn_delivery_remote_state(delivery_impl))
        if state == 0:
            return None
        elif state == cls.RECEIVED:
            return super().__new__(RemoteReceivedDisposition)
        elif state == cls.REJECTED:
            return super().__new__(RemoteRejectedDisposition)
        elif state == cls.MODIFIED:
            return super().__new__(RemoteModifiedDisposition)
        elif state == cls.TRANSACTIONAL_STATE:
            return super().__new__(RemoteTransactionalDisposition)
        else:
            return super().__new__(RemoteCustomDisposition)


class RemoteCustomDisposition(RemoteDisposition):

    def __init__(self, delivery_impl):
        impl = pn_custom_disposition(pn_delivery_remote(delivery_impl))
        self._type = DispositionType.or_int(pn_custom_disposition_get_type(impl))
        self._data = dat2obj(pn_custom_disposition_data(impl))

    @property
    def type(self) -> Union[int, DispositionType]:
        return self._type

    @property
    def data(self) -> Optional[Any]:
        """Access the disposition as a :class:`Data` object.

        Dispositions are an extension point in the AMQP protocol. The
        disposition interface provides setters/getters for those
        dispositions that are predefined by the specification, however
        access to the raw disposition data is provided so that other
        dispositions can be used.

        The :class:`Data` object returned by this operation is valid until
        the parent delivery is settled.
        """
        r = self._data
        return r if r != [] else None

    def apply_to(self, local_disposition: 'LocalDisposition'):
        CustomDisposition(self._type, self._data).apply_to(local_disposition)


class RemoteReceivedDisposition(RemoteDisposition):

    def __init__(self, delivery_impl):
        impl = pn_received_disposition(pn_delivery_remote(delivery_impl))
        self._section_number = pn_received_disposition_get_section_number(impl)
        self._section_offset = pn_received_disposition_get_section_offset(impl)

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.RECEIVED

    @property
    def section_number(self) -> int:
        return self._section_number

    @property
    def section_offset(self) -> int:
        return self._section_offset

    def apply_to(self, local_disposition: 'LocalDisposition'):
        ReceivedDisposition(self._section_number, self._section_offset).apply_to(local_disposition)


class RemoteRejectedDisposition(RemoteDisposition):

    def __init__(self, delivery_impl):
        impl = pn_rejected_disposition(pn_delivery_remote(delivery_impl))
        self._condition = cond2obj(pn_rejected_disposition_condition(impl))

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.REJECTED

    @property
    def condition(self) -> Optional[Condition]:
        return self._condition

    def apply_to(self, local_disposition: 'LocalDisposition'):
        RejectedDisposition(self._condition).apply_to(local_disposition)


class RemoteModifiedDisposition(RemoteDisposition):

    def __init__(self, delivery_impl):
        impl = pn_modified_disposition(pn_delivery_remote(delivery_impl))
        self._annotations = dat2obj(pn_modified_disposition_annotations(impl))
        self._failed = pn_modified_disposition_is_failed(impl)
        self._undeliverable = pn_modified_disposition_is_undeliverable(impl)

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.MODIFIED

    @property
    def failed(self) -> bool:
        return self._failed

    @property
    def undeliverable(self) -> bool:
        return self._undeliverable

    @property
    def annotations(self) -> Optional[dict['symbol', 'PythonAMQPData']]:
        return self._annotations

    def apply_to(self, local_disposition: 'LocalDisposition'):
        ModifiedDisposition(self._failed, self._undeliverable, self._annotations).apply_to(local_disposition)


class RemoteTransactionalDisposition(RemoteDisposition):

    def __init__(self, delivery_impl):
        impl = pn_transactional_disposition(pn_delivery_remote(delivery_impl))
        self._id = pn_transactional_disposition_get_id(impl)
        self._outcome_type = pn_transactional_disposition_get_outcome_type(impl)

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.TRANSACTIONAL_STATE

    @property
    def id(self):
        return self._id

    @property
    def outcome_type(self):
        return self._outcome_type

    def apply_to(self, local_disposition: 'LocalDisposition'):
        TransactionalDisposition(self._id, self._outcome_type).apply_to(local_disposition)


class LocalDisposition(Disposition):

    def __init__(self, delivery_impl):
        self._impl = pn_delivery_local(delivery_impl)
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
        return DispositionType.or_int(pn_disposition_type(self._impl))

    @property
    def data(self) -> Optional[Any]:
        return self._data

    @data.setter
    def data(self, obj: Any) -> None:
        self._data = obj

    @property
    def section_number(self) -> int:
        return pn_disposition_get_section_number(self._impl)

    @section_number.setter
    def section_number(self, n: int) -> None:
        pn_disposition_set_section_number(self._impl, n)

    @property
    def section_offset(self) -> int:
        return pn_disposition_get_section_offset(self._impl)

    @section_offset.setter
    def section_offset(self, n: int) -> None:
        pn_disposition_set_section_offset(self._impl, n)

    @property
    def condition(self) -> Optional[Condition]:
        return self._condition

    @condition.setter
    def condition(self, obj: Condition) -> None:
        self._condition = obj

    @property
    def failed(self) -> bool:
        return pn_disposition_is_failed(self._impl)

    @failed.setter
    def failed(self, b: bool) -> None:
        pn_disposition_set_failed(self._impl, b)

    @property
    def undeliverable(self) -> bool:
        return pn_disposition_is_undeliverable(self._impl)

    @undeliverable.setter
    def undeliverable(self, b: bool) -> None:
        pn_disposition_set_undeliverable(self._impl, b)

    @property
    def annotations(self) -> Optional[dict['symbol', 'PythonAMQPData']]:
        return self._annotations

    @annotations.setter
    def annotations(self, obj: dict['symbol', 'PythonAMQPData']) -> None:
        self._annotations = obj


class ReceivedDisposition(LocalDisposition):

    def __init__(self, section_number: int = None, section_offset: int = None):
        self._section_number = section_number
        self._section_offset = section_offset

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.RECEIVED

    @property
    def section_number(self) -> int:
        return self._section_number

    @section_number.setter
    def section_number(self, n: int) -> None:
        self._section_number = n

    @property
    def section_offset(self) -> int:
        return self._section_offset

    @section_offset.setter
    def section_offset(self, n: int) -> None:
        self._section_offset = n

    def apply_to(self, local_disposition: LocalDisposition):
        disp = pn_received_disposition(local_disposition._impl)
        if self._section_number:
            pn_received_disposition_set_section_number(disp, self._section_number)
        if self._section_offset:
            pn_received_disposition_set_section_offset(disp, self._section_offset)


class CustomDisposition(LocalDisposition):

    def __init__(self, type: int, data: Any = None):
        self._type = type
        self._data = data

    def apply_to(self, local_disposition: LocalDisposition):
        disp = pn_custom_disposition(local_disposition._impl)
        pn_custom_disposition_set_type(disp, self._type)
        obj2dat(self._data, pn_custom_disposition_data(disp))


class RejectedDisposition(LocalDisposition):

    def __init__(self, condition: Optional[Condition] = None):
        self._condition = condition

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.REJECTED

    @property
    def condition(self) -> Optional[Condition]:
        return self._condition

    @condition.setter
    def condition(self, obj: Condition) -> None:
        self._condition = obj

    def apply_to(self, local_disposition: LocalDisposition):
        disp = pn_rejected_disposition(local_disposition._impl)
        obj2cond(self._condition, pn_rejected_disposition_condition(disp))


class ModifiedDisposition(LocalDisposition):

    def __init__(self, failed: bool = True, undeliverable: bool = None,
                 annotations: Optional[dict['symbol', 'PythonAMQPData']] = None):
        self._failed = failed
        self._undeliverable = undeliverable
        self._annotations = annotations

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.MODIFIED

    @property
    def failed(self) -> bool:
        return self._failed

    @failed.setter
    def failed(self, b: bool) -> None:
        self._failed = b

    @property
    def undeliverable(self) -> bool:
        return self._undelivered

    @undeliverable.setter
    def undeliverable(self, b: bool) -> None:
        self._undelivered = b

    @property
    def annotations(self) -> Optional[dict['symbol', 'PythonAMQPData']]:
        return self._annotations

    @annotations.setter
    def annotations(self, obj: dict['symbol', 'PythonAMQPData']) -> None:
        self._annotations = obj

    def apply_to(self, local_disposition: LocalDisposition):
        disp = pn_modified_disposition(local_disposition._impl)
        if self._failed:
            pn_modified_disposition_set_failed(disp, self._failed)
        if self._undeliverable:
            pn_modified_disposition_set_undeliverable(disp, self._undeliverable)
        obj2dat(self._annotations, pn_modified_disposition_annotations(disp))


class TransactionalDisposition(LocalDisposition):

    def __init__(self, id, outcome_type=None):
        self._id = id
        self._outcome_type = outcome_type

    @property
    def type(self) -> Union[int, DispositionType]:
        return Disposition.TRANSACTIONAL_STATE

    @property
    def id(self):
        return self._id

    @id.setter
    def id(self, id):
        self._id = id

    @property
    def outcome_type(self):
        return self._outcome_type

    @outcome_type.setter
    def outcome_type(self, type):
        self._outcome_type = type

    def apply_to(self, local_disposition: LocalDisposition):
        disp = pn_transactional_disposition(local_disposition._impl)
        pn_transactional_disposition_set_id(disp, self._id)
        if self._outcome_type:
            pn_transactional_disposition_set_outcome_type(disp, self._outcome_type)


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

    get_context = pn_delivery_attachments

    def __init__(self, impl):
        if self.Uninitialized():
            self._local = None
            self._remote = None

    @property
    def remote(self) -> RemoteDisposition:
        if self._remote is None:
            self._remote = RemoteDisposition(self._impl)
        return self._remote

    @property
    def local(self) -> LocalDisposition:
        if self._local is None:
            self._local = LocalDisposition(self._impl)
        return self._local

    @local.setter
    def local(self, local_disposition: LocalDisposition) -> None:
        if self._local is None:
            self._local = LocalDisposition(self._impl)
        local_disposition.apply_to(self._local)

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
        ``False`` otherwise.
        """
        return pn_delivery_writable(self._impl)

    @property
    def readable(self) -> bool:
        """
        ``True`` for an incoming delivery that has data to read,
        ``False`` otherwise.
        """
        return pn_delivery_readable(self._impl)

    @property
    def updated(self) -> bool:
        """
        ``True`` if the state of the delivery has been updated
        (e.g. it has been settled and/or accepted, rejected etc.),
        ``False`` otherwise.
        """
        return pn_delivery_updated(self._impl)

    def update(self, state: Union[int, DispositionType, None] = None) -> None:
        """
        Set the local state of the delivery e.g. :const:`ACCEPTED`,
        :const:`REJECTED`, :const:`RELEASED`.

        :param state: State of delivery, if omitted we assume the delivery state is already set
        by other means
        """
        if state:
            if state == self.MODIFIED:
                obj2dat(self.local._annotations, pn_disposition_annotations(self.local._impl))
            elif state == self.REJECTED:
                obj2cond(self.local._condition, pn_disposition_condition(self.local._impl))
            elif state not in (self.ACCEPTED, self.RECEIVED, self.RELEASED):
                obj2dat(self.local._data, pn_disposition_data(self.local._impl))
            pn_delivery_update(self._impl, state)
        else:
            pn_delivery_update(self._impl, self.local_state)

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
    def local_state(self) -> Union[int, DispositionType]:
        """A local state of the delivery."""
        return DispositionType.or_int(pn_delivery_local_state(self._impl))

    @property
    def remote_state(self) -> Union[int, DispositionType]:
        """A remote state of the delivery as indicated by the remote peer."""
        return DispositionType.or_int(pn_delivery_remote_state(self._impl))

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
    def unsettled_next(self) -> Optional['Delivery']:
        """
        The next unsettled delivery on the link or ``None`` if there are
        no more unsettled deliveries.
        """
        return Delivery.wrap(pn_unsettled_next(self._impl))

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
    def transport(self) -> Optional[Transport]:
        """
        The :class:`Transport` bound to the :class:`Connection` over which
        the delivery was sent or received.
        """
        return self.connection.transport
