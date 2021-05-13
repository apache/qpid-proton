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

from cproton import PN_DEFAULT_PRIORITY, PN_OVERFLOW, pn_error_text, pn_message, \
    pn_message_annotations, pn_message_body, pn_message_clear, pn_message_correlation_id, pn_message_decode, \
    pn_message_encode, pn_message_error, pn_message_free, pn_message_get_address, pn_message_get_content_encoding, \
    pn_message_get_content_type, pn_message_get_creation_time, pn_message_get_delivery_count, \
    pn_message_get_expiry_time, pn_message_get_group_id, pn_message_get_group_sequence, pn_message_get_priority, \
    pn_message_get_reply_to, pn_message_get_reply_to_group_id, pn_message_get_subject, pn_message_get_ttl, \
    pn_message_get_user_id, pn_message_id, pn_message_instructions, pn_message_is_durable, \
    pn_message_is_first_acquirer, pn_message_is_inferred, pn_message_properties, pn_message_set_address, \
    pn_message_set_content_encoding, pn_message_set_content_type, pn_message_set_creation_time, \
    pn_message_set_delivery_count, pn_message_set_durable, pn_message_set_expiry_time, pn_message_set_first_acquirer, \
    pn_message_set_group_id, pn_message_set_group_sequence, pn_message_set_inferred, pn_message_set_priority, \
    pn_message_set_reply_to, pn_message_set_reply_to_group_id, pn_message_set_subject, \
    pn_message_set_ttl, pn_message_set_user_id

from ._common import isinteger, millis2secs, secs2millis, unicode2utf8, utf82unicode
from ._data import char, Data, symbol, ulong, AnnotationDict
from ._endpoints import Link
from ._exceptions import EXCEPTIONS, MessageException


class Message(object):
    """The :py:class:`Message` class is a mutable holder of message content.

    :ivar instructions: delivery instructions for the message ("Delivery Annotations" in the AMQP 1.0 spec)
    :vartype instructions: ``dict``
    :ivar ~.annotations: infrastructure defined message annotations ("Message Annotations" in the AMQP 1.0 spec)
    :vartype ~.annotations: ``dict``
    :ivar ~.properties: application defined message properties
    :vartype ~.properties: ``dict``
    :ivar body: message body
    :vartype body: bytes | unicode | dict | list | int | long | float | UUID

    :param kwargs: Message property name/value pairs to initialize the Message
    """

    DEFAULT_PRIORITY = PN_DEFAULT_PRIORITY
    """ Default AMQP message priority"""

    def __init__(self, body=None, **kwargs):
        self._msg = pn_message()
        self._id = Data(pn_message_id(self._msg))
        self._correlation_id = Data(pn_message_correlation_id(self._msg))
        self.instructions = None
        self.annotations = None
        self.properties = None
        self.body = body
        for k, v in kwargs.items():
            getattr(self, k)  # Raise exception if it's not a valid attribute.
            setattr(self, k, v)

    def __del__(self):
        if hasattr(self, "_msg"):
            pn_message_free(self._msg)
            del self._msg

    def _check(self, err):
        if err < 0:
            exc = EXCEPTIONS.get(err, MessageException)
            raise exc("[%s]: %s" % (err, pn_error_text(pn_message_error(self._msg))))
        else:
            return err

    def _check_property_keys(self):
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

    def _pre_encode(self):
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

    def _post_decode(self):
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

    def clear(self):
        """
        Clears the contents of the :class:`Message`. All fields will be reset to
        their default values.
        """
        pn_message_clear(self._msg)
        self.instructions = None
        self.annotations = None
        self.properties = None
        self.body = None

    def _is_inferred(self):
        return pn_message_is_inferred(self._msg)

    def _set_inferred(self, value):
        self._check(pn_message_set_inferred(self._msg, bool(value)))

    inferred = property(_is_inferred, _set_inferred, doc="""
        The inferred flag for a message indicates how the message content
        is encoded into AMQP sections. If inferred is true then binary and
        list values in the body of the message will be encoded as AMQP DATA
        and AMQP SEQUENCE sections, respectively. If inferred is false,
        then all values in the body of the message will be encoded as AMQP
        VALUE sections regardless of their type.

        :type: ``bool``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _is_durable(self):
        return pn_message_is_durable(self._msg)

    def _set_durable(self, value):
        self._check(pn_message_set_durable(self._msg, bool(value)))

    durable = property(_is_durable, _set_durable, doc="""
        The durable property indicates that the message should be held durably
        by any intermediaries taking responsibility for the message.

        :type: ``bool``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_priority(self):
        return pn_message_get_priority(self._msg)

    def _set_priority(self, value):
        self._check(pn_message_set_priority(self._msg, value))

    priority = property(_get_priority, _set_priority, doc="""
        The relative priority of the message, with higher numbers indicating
        higher priority. The number of available priorities depends
        on the implementation, but AMQP defines the default priority as
        the value ``4``. See the
        `OASIS AMQP 1.0 standard
        <http://docs.oasis-open.org/amqp/core/v1.0/os/amqp-core-messaging-v1.0-os.html#type-header>`_
        for more details on message priority.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_ttl(self):
        return millis2secs(pn_message_get_ttl(self._msg))

    def _set_ttl(self, value):
        self._check(pn_message_set_ttl(self._msg, secs2millis(value)))

    ttl = property(_get_ttl, _set_ttl, doc="""
        The time to live of the message measured in seconds. Expired messages
        may be dropped.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _is_first_acquirer(self):
        return pn_message_is_first_acquirer(self._msg)

    def _set_first_acquirer(self, value):
        self._check(pn_message_set_first_acquirer(self._msg, bool(value)))

    first_acquirer = property(_is_first_acquirer, _set_first_acquirer, doc="""
        ``True`` iff the recipient is the first to acquire the message,
        ``False`` otherwise.

        :type: ``bool``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_delivery_count(self):
        return pn_message_get_delivery_count(self._msg)

    def _set_delivery_count(self, value):
        self._check(pn_message_set_delivery_count(self._msg, value))

    delivery_count = property(_get_delivery_count, _set_delivery_count, doc="""
        The number of delivery attempts made for this message.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_id(self):
        return self._id.get_object()

    def _set_id(self, value):
        if isinteger(value):
            value = ulong(value)
        self._id.rewind()
        self._id.put_object(value)

    id = property(_get_id, _set_id, doc="""
        The globally unique id of the message, and can be used
        to determine if a received message is a duplicate. The allowed
        types to set the id are:

        :type: The valid AMQP types for an id are one of:

               * ``int`` (unsigned)
               * ``uuid.UUID``
               * ``bytes``
               * ``str``
        """)

    def _get_user_id(self):
        return pn_message_get_user_id(self._msg)

    def _set_user_id(self, value):
        self._check(pn_message_set_user_id(self._msg, value))

    user_id = property(_get_user_id, _set_user_id, doc="""
        The user id of the message creator.

        :type: ``bytes``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_address(self):
        return utf82unicode(pn_message_get_address(self._msg))

    def _set_address(self, value):
        self._check(pn_message_set_address(self._msg, unicode2utf8(value)))

    address = property(_get_address, _set_address, doc="""
        The address of the message.

        :type: ``str``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_subject(self):
        return utf82unicode(pn_message_get_subject(self._msg))

    def _set_subject(self, value):
        self._check(pn_message_set_subject(self._msg, unicode2utf8(value)))

    subject = property(_get_subject, _set_subject, doc="""
        The subject of the message.

        :type: ``str``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_reply_to(self):
        return utf82unicode(pn_message_get_reply_to(self._msg))

    def _set_reply_to(self, value):
        self._check(pn_message_set_reply_to(self._msg, unicode2utf8(value)))

    reply_to = property(_get_reply_to, _set_reply_to, doc="""
        The reply-to address for the message.

        :type: ``str``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_correlation_id(self):
        return self._correlation_id.get_object()

    def _set_correlation_id(self, value):
        if isinteger(value):
            value = ulong(value)
        self._correlation_id.rewind()
        self._correlation_id.put_object(value)

    correlation_id = property(_get_correlation_id, _set_correlation_id, doc="""
        The correlation-id for the message.

        :type: The valid AMQP types for a correlation-id are one of:

               * ``int`` (unsigned)
               * ``uuid.UUID``
               * ``bytes``
               * ``str``
        """)

    def _get_content_type(self):
        return symbol(utf82unicode(pn_message_get_content_type(self._msg)))

    def _set_content_type(self, value):
        self._check(pn_message_set_content_type(self._msg, unicode2utf8(value)))

    content_type = property(_get_content_type, _set_content_type, doc="""
        The RFC-2046 [RFC2046] MIME type for the message body.

        :type: :class:`symbol`
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_content_encoding(self):
        return symbol(utf82unicode(pn_message_get_content_encoding(self._msg)))

    def _set_content_encoding(self, value):
        self._check(pn_message_set_content_encoding(self._msg, unicode2utf8(value)))

    content_encoding = property(_get_content_encoding, _set_content_encoding, doc="""
        The content-encoding of the message.

        :type: :class:`symbol`
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_expiry_time(self):
        return millis2secs(pn_message_get_expiry_time(self._msg))

    def _set_expiry_time(self, value):
        self._check(pn_message_set_expiry_time(self._msg, secs2millis(value)))

    expiry_time = property(_get_expiry_time, _set_expiry_time, doc="""
        The absolute expiry time of the message in seconds using the Unix time_t [IEEE1003] encoding.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_creation_time(self):
        return millis2secs(pn_message_get_creation_time(self._msg))

    def _set_creation_time(self, value):
        self._check(pn_message_set_creation_time(self._msg, secs2millis(value)))

    creation_time = property(_get_creation_time, _set_creation_time, doc="""
        The creation time of the message in seconds using the Unix time_t [IEEE1003] encoding.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_group_id(self):
        return utf82unicode(pn_message_get_group_id(self._msg))

    def _set_group_id(self, value):
        self._check(pn_message_set_group_id(self._msg, unicode2utf8(value)))

    group_id = property(_get_group_id, _set_group_id, doc="""
        The group id of the message.

        :type: ``str``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_group_sequence(self):
        return pn_message_get_group_sequence(self._msg)

    def _set_group_sequence(self, value):
        self._check(pn_message_set_group_sequence(self._msg, value))

    group_sequence = property(_get_group_sequence, _set_group_sequence, doc="""
        The sequence of the message within its group.

        :type: ``int``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_reply_to_group_id(self):
        return utf82unicode(pn_message_get_reply_to_group_id(self._msg))

    def _set_reply_to_group_id(self, value):
        self._check(pn_message_set_reply_to_group_id(self._msg, unicode2utf8(value)))

    reply_to_group_id = property(_get_reply_to_group_id, _set_reply_to_group_id, doc="""
        The group-id for any replies.

        :type: ``str``
        :raise: :exc:`MessageException` if there is any Proton error when using the setter.
        """)

    def _get_instructions(self):
        return self.instruction_dict

    def _set_instructions(self, instructions):
        if isinstance(instructions, dict):
            self.instruction_dict = AnnotationDict(instructions, raise_on_error=False)
        else:
            self.instruction_dict = instructions

    instructions = property(_get_instructions, _set_instructions, doc="""
    Delivery annotations as a dictionary of key/values. The AMQP 1.0
    specification restricts this dictionary to have keys that are either
    :class:`symbol` or :class:`ulong` types. It is possible to use
    the special ``dict`` subclass :class:`AnnotationDict` which
    will by default enforce these restrictions on construction. In addition,
    if string types are used, this class will be silently convert them into
    symbols.

    :type: :class:`AnnotationDict`. Any ``dict`` with :class:`ulong` or :class:`symbol` keys.
    """)

    def _get_annotations(self):
        return self.annotation_dict

    def _set_annotations(self, annotations):
        if isinstance(annotations, dict):
            self.annotation_dict = AnnotationDict(annotations, raise_on_error=False)
        else:
            self.annotation_dict = annotations

    annotations = property(_get_annotations, _set_annotations, doc="""
    Message annotations as a dictionary of key/values. The AMQP 1.0
    specification restricts this dictionary to have keys that are either
    :class:`symbol` or :class:`ulong` types. It is possible to use
    the special ``dict`` subclass :class:`AnnotationDict` which
    will by default enforce these restrictions on construction. In addition,
    if a string types are used, this class will silently convert them into
    symbols.

    :type: :class:`AnnotationDict`. Any ``dict`` with :class:`ulong` or :class:`symbol` keys.
    """)

    def encode(self):
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

    def decode(self, data):
        self._check(pn_message_decode(self._msg, data))
        self._post_decode()

    def send(self, sender, tag=None):
        """
        Encodes and sends the message content using the specified sender,
        and, if present, using the specified tag. Upon success, will
        return the :class:`Delivery` object for the sent message.

        :param sender: The sender to send the message
        :type sender: :class:`Sender`
        :param tag: The delivery tag for the sent message
        :type tag: ``bytes``
        :return: The delivery associated with the sent message
        :rtype: :class:`Delivery`
        """
        dlv = sender.delivery(tag or sender.delivery_tag())
        encoded = self.encode()
        sender.stream(encoded)
        sender.advance()
        if sender.snd_settle_mode == Link.SND_SETTLED:
            dlv.settle()
        return dlv

    def recv(self, link):
        """
        Receives and decodes the message content for the current :class:`Delivery`
        from the link. Upon success it will return the current delivery
        for the link. If there is no current delivery, or if the current
        delivery is incomplete, or if the link is not a receiver, it will
        return ``None``.

        :param link: The link to receive a message from
        :type link: :class:`Link`
        :return: the delivery associated with the decoded message (or None)
        :rtype: :class:`Delivery`

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

    def __repr__(self):
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
