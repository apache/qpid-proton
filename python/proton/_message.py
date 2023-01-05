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

from cproton import PN_DEFAULT_PRIORITY, PN_STRING, PN_UUID, PN_OVERFLOW, pn_error_text, pn_message, \
    pn_message_annotations, pn_message_body, pn_message_clear, pn_message_decode, \
    pn_message_encode, pn_message_error, pn_message_free, pn_message_get_address, pn_message_get_content_encoding, \
    pn_message_get_content_type, pn_message_get_correlation_id, pn_message_get_creation_time, pn_message_get_delivery_count, \
    pn_message_get_expiry_time, pn_message_get_group_id, pn_message_get_group_sequence, pn_message_get_id, pn_message_get_priority, \
    pn_message_get_reply_to, pn_message_get_reply_to_group_id, pn_message_get_subject, pn_message_get_ttl, \
    pn_message_get_user_id, pn_message_instructions, pn_message_is_durable, \
    pn_message_is_first_acquirer, pn_message_is_inferred, pn_message_properties, pn_message_set_address, \
    pn_message_set_content_encoding, pn_message_set_content_type, pn_message_set_correlation_id, pn_message_set_creation_time, \
    pn_message_set_delivery_count, pn_message_set_durable, pn_message_set_expiry_time, pn_message_set_first_acquirer, \
    pn_message_set_group_id, pn_message_set_group_sequence, pn_message_set_id, pn_message_set_inferred, pn_message_set_priority, \
    pn_message_set_reply_to, pn_message_set_reply_to_group_id, pn_message_set_subject, \
    pn_message_set_ttl, pn_message_set_user_id

from ._common import millis2secs, secs2millis
from ._data import char, Data, symbol, ulong, AnnotationDict
from ._endpoints import Link
from ._exceptions import EXCEPTIONS, MessageException
from uuid import UUID
from typing import Dict, Optional, Union, TYPE_CHECKING, overload

if TYPE_CHECKING:
    from proton._delivery import Delivery
    from proton._endpoints import Sender, Receiver
    from proton._data import Described, PythonAMQPData


class Message(object):
    """The :py:class:`Message` class is a mutable holder of message content.

    :ivar instructions: delivery instructions for the message ("Delivery Annotations" in the AMQP 1.0 spec)
    :vartype instructions: ``dict``
    :ivar ~.annotations: infrastructure defined message annotations ("Message Annotations" in the AMQP 1.0 spec)
    :vartype ~.annotations: ``dict``
    :ivar ~.properties: application defined message properties
    :vartype ~.properties: ``dict``
    :ivar body: message body

    :param kwargs: Message property name/value pairs to initialize the Message
    """

    DEFAULT_PRIORITY = PN_DEFAULT_PRIORITY
    """ Default AMQP message priority"""

    def __init__(
            self,
            body: Union[bytes, str, dict, list, int, float, 'UUID', 'Described', None] = None,
            **kwargs
    ) -> None:
        self._msg = pn_message()
        self.instructions = None
        self.annotations = None
        self.properties = None
        self.body = body
        for k, v in kwargs.items():
            getattr(self, k)  # Raise exception if it's not a valid attribute.
            setattr(self, k, v)

    def __del__(self) -> None:
        if hasattr(self, "_msg"):
            pn_message_free(self._msg)
            del self._msg

    def _check(self, err: int) -> int:
        if err < 0:
            exc = EXCEPTIONS.get(err, MessageException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_message_error(self._msg))))
        else:
            return err

    def _check_property_keys(self) -> None:
        """
        AMQP allows only string keys for properties. This function checks that this requirement is met
        and raises a MessageException if not. However, in certain cases, conversions to string are
        automatically performed:

        1. When a key is a user-defined (non-AMQP) subclass of str.
           AMQP types symbol and char, although derived from str, are not converted,
           and result in an exception.
        """
        # We cannot make changes to the dict while iterating, so we
        # must save and make the changes afterwards
        changed_keys = []
        for k in self.properties.keys():
            if isinstance(k, str):
                # strings and their subclasses
                if type(k) is symbol or type(k) is char:
                    # Exclude symbol and char
                    raise MessageException('Application property key is not string type: key=%s %s' % (str(k), type(k)))
                if type(k) is not str:
                    # Only for string subclasses, convert to string
                    changed_keys.append((k, str(k)))
            else:
                # Anything else: raise exception
                raise MessageException('Application property key is not string type: key=%s %s' % (str(k), type(k)))
        # Make the key changes
        for old_key, new_key in changed_keys:
            self.properties[new_key] = self.properties.pop(old_key)

    def _pre_encode(self) -> None:
        inst = Data(pn_message_instructions(self._msg))
        ann = Data(pn_message_annotations(self._msg))
        props = Data(pn_message_properties(self._msg))
        body = Data(pn_message_body(self._msg))

        inst.clear()
        if self.instructions is not None:
            inst.put_object(self.instructions)
        ann.clear()
        if self.annotations is not None:
            ann.put_object(self.annotations)
        props.clear()
        if self.properties is not None:
            self._check_property_keys()
            props.put_object(self.properties)
        body.clear()
        if self.body is not None:
            body.put_object(self.body)

    def _post_decode(self) -> None:
        inst = Data(pn_message_instructions(self._msg))
        ann = Data(pn_message_annotations(self._msg))
        props = Data(pn_message_properties(self._msg))
        body = Data(pn_message_body(self._msg))

        if inst.next():
            self.instructions = inst.get_object()
        else:
            self.instructions = None
        if ann.next():
            self.annotations = ann.get_object()
        else:
            self.annotations = None
        if props.next():
            self.properties = props.get_object()
        else:
            self.properties = None
        if body.next():
            self.body = body.get_object()
        else:
            self.body = None

    def clear(self) -> None:
        """
        Clears the contents of the :class:`Message`. All fields will be reset to
        their default values.
        """
        pn_message_clear(self._msg)
        self.instructions = None
        self.annotations = None
        self.properties = None
        self.body = None

    @property
    def inferred(self) -> bool:
        """The inferred flag for a message indicates how the message content
        is encoded into AMQP sections. If inferred is true then binary and
        list values in the body of the message will be encoded as AMQP DATA
        and AMQP SEQUENCE sections, respectively. If inferred is false,
        then all values in the body of the message will be encoded as AMQP
        VALUE sections regardless of their type.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_is_inferred(self._msg)

    @inferred.setter
    def inferred(self, value: bool) -> None:
        self._check(pn_message_set_inferred(self._msg, bool(value)))

    @property
    def durable(self) -> bool:
        """The durable property indicates that the message should be held durably
        by any intermediaries taking responsibility for the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_is_durable(self._msg)

    @durable.setter
    def durable(self, value: bool) -> None:
        self._check(pn_message_set_durable(self._msg, bool(value)))

    @property
    def priority(self) -> int:
        """The relative priority of the message, with higher numbers indicating
        higher priority. The number of available priorities depends
        on the implementation, but AMQP defines the default priority as
        the value ``4``. See the
        `OASIS AMQP 1.0 standard
        <http://docs.oasis-open.org/amqp/core/v1.0/os/amqp-core-messaging-v1.0-os.html#type-header>`_
        for more details on message priority.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_priority(self._msg)

    @priority.setter
    def priority(self, value: int) -> None:
        self._check(pn_message_set_priority(self._msg, value))

    @property
    def ttl(self) -> float:
        """The time to live of the message measured in seconds. Expired messages
        may be dropped.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return millis2secs(pn_message_get_ttl(self._msg))

    @ttl.setter
    def ttl(self, value: Union[float, int]) -> None:
        self._check(pn_message_set_ttl(self._msg, secs2millis(value)))

    @property
    def first_acquirer(self) -> bool:
        """``True`` iff the recipient is the first to acquire the message,
        ``False`` otherwise.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_is_first_acquirer(self._msg)

    @first_acquirer.setter
    def first_acquirer(self, value: bool) -> None:
        self._check(pn_message_set_first_acquirer(self._msg, bool(value)))

    @property
    def delivery_count(self) -> int:
        """The number of delivery attempts made for this message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_delivery_count(self._msg)

    @delivery_count.setter
    def delivery_count(self, value: int) -> None:
        self._check(pn_message_set_delivery_count(self._msg, value))

    @property
    def id(self) -> Optional[Union[str, bytes, 'UUID', ulong]]:
        """The globally unique id of the message, and can be used
        to determine if a received message is a duplicate. The allowed
        types to set the id are:

        :type: The valid AMQP types for an id are one of:

               * ``int`` (unsigned)
               * ``uuid.UUID``
               * ``bytes``
               * ``str``
        """
        value = pn_message_get_id(self._msg)
        if isinstance(value, tuple):
            if value[0] == PN_UUID:
                value = UUID(bytes=value[1])
        return value

    @id.setter
    def id(self, value: Optional[Union[str, bytes, 'UUID', int]]) -> None:
        pn_message_set_id(self._msg, value)

    @property
    def user_id(self) -> bytes:
        """The user id of the message creator.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_user_id(self._msg)

    @user_id.setter
    def user_id(self, value: bytes) -> None:
        self._check(pn_message_set_user_id(self._msg, value))

    @property
    def address(self) -> Optional[str]:
        """The address of the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_address(self._msg)

    @address.setter
    def address(self, value: str) -> None:
        self._check(pn_message_set_address(self._msg, value))

    @property
    def subject(self) -> Optional[str]:
        """The subject of the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_subject(self._msg)

    @subject.setter
    def subject(self, value: str) -> None:
        self._check(pn_message_set_subject(self._msg, value))

    @property
    def reply_to(self) -> Optional[str]:
        """The reply-to address for the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_reply_to(self._msg)

    @reply_to.setter
    def reply_to(self, value: str) -> None:
        self._check(pn_message_set_reply_to(self._msg, value))

    @property
    def correlation_id(self) -> Optional[Union['UUID', ulong, str, bytes]]:
        """The correlation-id for the message.

        :type: The valid AMQP types for a correlation-id are one of:

               * ``int`` (unsigned)
               * ``uuid.UUID``
               * ``bytes``
               * ``str``
        """
        value = pn_message_get_correlation_id(self._msg)
        if isinstance(value, tuple):
            if value[0] == PN_UUID:
                value = UUID(bytes=value[1])
        return value

    @correlation_id.setter
    def correlation_id(self, value: Optional[Union[str, bytes, 'UUID', int]]) -> None:
        pn_message_set_correlation_id(self._msg, value)

    @property
    def content_type(self) -> symbol:
        """The RFC-2046 [RFC2046] MIME type for the message body.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return symbol(pn_message_get_content_type(self._msg))

    @content_type.setter
    def content_type(self, value: str) -> None:
        self._check(pn_message_set_content_type(self._msg, value))

    @property
    def content_encoding(self) -> symbol:
        """The content-encoding of the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return symbol(pn_message_get_content_encoding(self._msg))

    @content_encoding.setter
    def content_encoding(self, value: str) -> None:
        self._check(pn_message_set_content_encoding(self._msg, value))

    @property
    def expiry_time(self) -> float:  # TODO doc said int
        """The absolute expiry time of the message in seconds using the Unix time_t [IEEE1003] encoding.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return millis2secs(pn_message_get_expiry_time(self._msg))

    @expiry_time.setter
    def expiry_time(self, value: Union[float, int]) -> None:
        self._check(pn_message_set_expiry_time(self._msg, secs2millis(value)))

    @property
    def creation_time(self) -> float:
        """The creation time of the message in seconds using the Unix time_t [IEEE1003] encoding.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return millis2secs(pn_message_get_creation_time(self._msg))

    @creation_time.setter
    def creation_time(self, value: Union[float, int]) -> None:
        self._check(pn_message_set_creation_time(self._msg, secs2millis(value)))

    @property
    def group_id(self) -> Optional[str]:
        """The group id of the message.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_group_id(self._msg)

    @group_id.setter
    def group_id(self, value: str) -> None:
        self._check(pn_message_set_group_id(self._msg, value))

    @property
    def group_sequence(self) -> int:
        """The sequence of the message within its group.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_group_sequence(self._msg)

    @group_sequence.setter
    def group_sequence(self, value: int) -> None:
        self._check(pn_message_set_group_sequence(self._msg, value))

    @property
    def reply_to_group_id(self) -> Optional[str]:
        """The group-id for any replies.

        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """
        return pn_message_get_reply_to_group_id(self._msg)

    @reply_to_group_id.setter
    def reply_to_group_id(self, value: str) -> None:
        self._check(pn_message_set_reply_to_group_id(self._msg, value))

    @property
    def instructions(self) -> Optional[AnnotationDict]:
        """Delivery annotations as a dictionary of key/values. The AMQP 1.0
        specification restricts this dictionary to have keys that are either
        :class:`symbol` or :class:`ulong` types. It is possible to use
        the special ``dict`` subclass :class:`AnnotationDict` which
        will by default enforce these restrictions on construction. In addition,
        if string types are used, this class will be silently convert them into
        symbols.

        :type: :class:`AnnotationDict`. Any ``dict`` with :class:`ulong` or :class:`symbol` keys.
        """
        return self.instruction_dict

    @instructions.setter
    def instructions(self, instructions: Optional[Dict[Union[str, int], 'PythonAMQPData']]) -> None:
        if isinstance(instructions, dict):
            self.instruction_dict = AnnotationDict(instructions, raise_on_error=False)
        else:
            self.instruction_dict = instructions

    @property
    def annotations(self) -> Optional[AnnotationDict]:
        """Message annotations as a dictionary of key/values. The AMQP 1.0
        specification restricts this dictionary to have keys that are either
        :class:`symbol` or :class:`ulong` types. It is possible to use
        the special ``dict`` subclass :class:`AnnotationDict` which
        will by default enforce these restrictions on construction. In addition,
        if a string types are used, this class will silently convert them into
        symbols.

        :type: :class:`AnnotationDict`. Any ``dict`` with :class:`ulong` or :class:`symbol` keys.
        """
        return self.annotation_dict

    @annotations.setter
    def annotations(self, annotations: Optional[Dict[Union[str, int], 'PythonAMQPData']]) -> None:
        if isinstance(annotations, dict):
            self.annotation_dict = AnnotationDict(annotations, raise_on_error=False)
        else:
            self.annotation_dict = annotations

    def encode(self) -> bytes:
        self._pre_encode()
        sz = 16
        while True:
            err, data = pn_message_encode(self._msg, sz)
            if err == PN_OVERFLOW:
                sz *= 2
                continue
            else:
                self._check(err)
                return data

    def decode(self, data: bytes) -> None:
        self._check(pn_message_decode(self._msg, data))
        self._post_decode()

    def send(self, sender: 'Sender', tag: Optional[str] = None) -> 'Delivery':
        """
        Encodes and sends the message content using the specified sender,
        and, if present, using the specified tag. Upon success, will
        return the :class:`Delivery` object for the sent message.

        :param sender: The sender to send the message
        :param tag: The delivery tag for the sent message
        :return: The delivery associated with the sent message
        """
        dlv = sender.delivery(tag or sender.delivery_tag())
        encoded = self.encode()
        sender.stream(encoded)
        sender.advance()
        if sender.snd_settle_mode == Link.SND_SETTLED:
            dlv.settle()
        return dlv

    @overload
    def recv(self, link: 'Sender') -> None:
        ...

    def recv(self, link: 'Receiver') -> Optional['Delivery']:
        """
        Receives and decodes the message content for the current :class:`Delivery`
        from the link. Upon success it will return the current delivery
        for the link. If there is no current delivery, or if the current
        delivery is incomplete, or if the link is not a receiver, it will
        return ``None``.

        :param link: The link to receive a message from
        :return: the delivery associated with the decoded message (or None)

        """
        if link.is_sender:
            return None
        dlv = link.current
        if not dlv or dlv.partial:
            return None
        dlv.encoded = link.recv(dlv.pending)
        link.advance()
        # the sender has already forgotten about the delivery, so we might
        # as well too
        if link.remote_snd_settle_mode == Link.SND_SETTLED:
            dlv.settle()
        self.decode(dlv.encoded)
        return dlv

    def __repr__(self) -> str:
        props = []
        for attr in ("inferred", "address", "reply_to", "durable", "ttl",
                     "priority", "first_acquirer", "delivery_count", "id",
                     "correlation_id", "user_id", "group_id", "group_sequence",
                     "reply_to_group_id", "instructions", "annotations",
                     "properties", "body"):
            value = getattr(self, attr)
            if value:
                props.append("%s=%r" % (attr, value))
        return "Message(%s)" % ", ".join(props)
