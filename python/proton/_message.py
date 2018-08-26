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

from cproton import PN_DEFAULT_PRIORITY, PN_OVERFLOW, \
    pn_message_set_delivery_count, pn_message_set_address, pn_message_properties, \
    pn_message_get_user_id, pn_message_set_content_encoding, pn_message_get_subject, pn_message_get_priority, \
    pn_message_get_content_encoding, pn_message_body, \
    pn_message_correlation_id, pn_message_get_address, pn_message_set_content_type, pn_message_get_group_id, \
    pn_message_set_expiry_time, pn_message_set_creation_time, pn_message_error, \
    pn_message_is_first_acquirer, pn_message_set_priority, \
    pn_message_free, pn_message_get_creation_time, pn_message_is_inferred, pn_message_set_subject, \
    pn_message_set_user_id, pn_message_set_group_id, \
    pn_message_id, pn_message_clear, pn_message_set_durable, \
    pn_message_set_first_acquirer, pn_message_get_delivery_count, \
    pn_message_decode, pn_message_set_reply_to_group_id, \
    pn_message_get_group_sequence, pn_message_set_reply_to, \
    pn_message_set_ttl, pn_message_get_reply_to, pn_message, pn_message_annotations, pn_message_is_durable, \
    pn_message_instructions, pn_message_get_content_type, \
    pn_message_get_reply_to_group_id, pn_message_get_ttl, pn_message_encode, pn_message_get_expiry_time, \
    pn_message_set_group_sequence, pn_message_set_inferred, \
    pn_inspect, pn_string, pn_string_get, pn_free, pn_error_text

from . import _compat
from ._common import Constant, isinteger, secs2millis, millis2secs, unicode2utf8, utf82unicode
from ._data import Data, ulong, symbol
from ._endpoints import Link
from ._exceptions import EXCEPTIONS, MessageException

#
# Hack to provide Python2 <---> Python3 compatibility
try:
    unicode()
except NameError:
    unicode = str



class Message(object):
    """The L{Message} class is a mutable holder of message content.

    @ivar instructions: delivery instructions for the message
    @type instructions: dict
    @ivar annotations: infrastructure defined message annotations
    @type annotations: dict
    @ivar properties: application defined message properties
    @type properties: dict
    @ivar body: message body
    @type body: bytes | unicode | dict | list | int | long | float | UUID
    """

    DEFAULT_PRIORITY = PN_DEFAULT_PRIORITY

    def __init__(self, body=None, **kwargs):
        """
        @param kwargs: Message property name/value pairs to initialise the Message
        """
        self._msg = pn_message()
        self._id = Data(pn_message_id(self._msg))
        self._correlation_id = Data(pn_message_correlation_id(self._msg))
        self.instructions = None
        self.annotations = None
        self.properties = None
        self.body = body
        for k, v in _compat.iteritems(kwargs):
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
        for k in self.properties.keys():
            if isinstance(k, unicode):
                # py2 unicode, py3 str (via hack definition)
                continue
            # If key is binary then change to string
            elif isinstance(k, str):
                # py2 str
                self.properties[k.encode('utf-8')] = self.properties.pop(k)
            else:
                raise MessageException('Application property key is not string type: key=%s %s' % (str(k), type(k)))

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
        Clears the contents of the L{Message}. All fields will be reset to
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
""")

    def _is_durable(self):
        return pn_message_is_durable(self._msg)

    def _set_durable(self, value):
        self._check(pn_message_set_durable(self._msg, bool(value)))

    durable = property(_is_durable, _set_durable,
                       doc="""
The durable property indicates that the message should be held durably
by any intermediaries taking responsibility for the message.
""")

    def _get_priority(self):
        return pn_message_get_priority(self._msg)

    def _set_priority(self, value):
        self._check(pn_message_set_priority(self._msg, value))

    priority = property(_get_priority, _set_priority,
                        doc="""
The priority of the message.
""")

    def _get_ttl(self):
        return millis2secs(pn_message_get_ttl(self._msg))

    def _set_ttl(self, value):
        self._check(pn_message_set_ttl(self._msg, secs2millis(value)))

    ttl = property(_get_ttl, _set_ttl,
                   doc="""
The time to live of the message measured in seconds. Expired messages
may be dropped.
""")

    def _is_first_acquirer(self):
        return pn_message_is_first_acquirer(self._msg)

    def _set_first_acquirer(self, value):
        self._check(pn_message_set_first_acquirer(self._msg, bool(value)))

    first_acquirer = property(_is_first_acquirer, _set_first_acquirer,
                              doc="""
True iff the recipient is the first to acquire the message.
""")

    def _get_delivery_count(self):
        return pn_message_get_delivery_count(self._msg)

    def _set_delivery_count(self, value):
        self._check(pn_message_set_delivery_count(self._msg, value))

    delivery_count = property(_get_delivery_count, _set_delivery_count,
                              doc="""
The number of delivery attempts made for this message.
""")

    def _get_id(self):
        return self._id.get_object()

    def _set_id(self, value):
        if isinteger(value):
            value = ulong(value)
        self._id.rewind()
        self._id.put_object(value)

    id = property(_get_id, _set_id,
                  doc="""
The id of the message.
""")

    def _get_user_id(self):
        return pn_message_get_user_id(self._msg)

    def _set_user_id(self, value):
        self._check(pn_message_set_user_id(self._msg, value))

    user_id = property(_get_user_id, _set_user_id,
                       doc="""
The user id of the message creator.
""")

    def _get_address(self):
        return utf82unicode(pn_message_get_address(self._msg))

    def _set_address(self, value):
        self._check(pn_message_set_address(self._msg, unicode2utf8(value)))

    address = property(_get_address, _set_address,
                       doc="""
The address of the message.
""")

    def _get_subject(self):
        return utf82unicode(pn_message_get_subject(self._msg))

    def _set_subject(self, value):
        self._check(pn_message_set_subject(self._msg, unicode2utf8(value)))

    subject = property(_get_subject, _set_subject,
                       doc="""
The subject of the message.
""")

    def _get_reply_to(self):
        return utf82unicode(pn_message_get_reply_to(self._msg))

    def _set_reply_to(self, value):
        self._check(pn_message_set_reply_to(self._msg, unicode2utf8(value)))

    reply_to = property(_get_reply_to, _set_reply_to,
                        doc="""
The reply-to address for the message.
""")

    def _get_correlation_id(self):
        return self._correlation_id.get_object()

    def _set_correlation_id(self, value):
        if isinteger(value):
            value = ulong(value)
        self._correlation_id.rewind()
        self._correlation_id.put_object(value)

    correlation_id = property(_get_correlation_id, _set_correlation_id,
                              doc="""
The correlation-id for the message.
""")

    def _get_content_type(self):
        return symbol(utf82unicode(pn_message_get_content_type(self._msg)))

    def _set_content_type(self, value):
        self._check(pn_message_set_content_type(self._msg, unicode2utf8(value)))

    content_type = property(_get_content_type, _set_content_type,
                            doc="""
The content-type of the message.
""")

    def _get_content_encoding(self):
        return symbol(utf82unicode(pn_message_get_content_encoding(self._msg)))

    def _set_content_encoding(self, value):
        self._check(pn_message_set_content_encoding(self._msg, unicode2utf8(value)))

    content_encoding = property(_get_content_encoding, _set_content_encoding,
                                doc="""
The content-encoding of the message.
""")

    def _get_expiry_time(self):
        return millis2secs(pn_message_get_expiry_time(self._msg))

    def _set_expiry_time(self, value):
        self._check(pn_message_set_expiry_time(self._msg, secs2millis(value)))

    expiry_time = property(_get_expiry_time, _set_expiry_time,
                           doc="""
The expiry time of the message.
""")

    def _get_creation_time(self):
        return millis2secs(pn_message_get_creation_time(self._msg))

    def _set_creation_time(self, value):
        self._check(pn_message_set_creation_time(self._msg, secs2millis(value)))

    creation_time = property(_get_creation_time, _set_creation_time,
                             doc="""
The creation time of the message.
""")

    def _get_group_id(self):
        return utf82unicode(pn_message_get_group_id(self._msg))

    def _set_group_id(self, value):
        self._check(pn_message_set_group_id(self._msg, unicode2utf8(value)))

    group_id = property(_get_group_id, _set_group_id,
                        doc="""
The group id of the message.
""")

    def _get_group_sequence(self):
        return pn_message_get_group_sequence(self._msg)

    def _set_group_sequence(self, value):
        self._check(pn_message_set_group_sequence(self._msg, value))

    group_sequence = property(_get_group_sequence, _set_group_sequence,
                              doc="""
The sequence of the message within its group.
""")

    def _get_reply_to_group_id(self):
        return utf82unicode(pn_message_get_reply_to_group_id(self._msg))

    def _set_reply_to_group_id(self, value):
        self._check(pn_message_set_reply_to_group_id(self._msg, unicode2utf8(value)))

    reply_to_group_id = property(_get_reply_to_group_id, _set_reply_to_group_id,
                                 doc="""
The group-id for any replies.
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
        dlv = sender.delivery(tag or sender.delivery_tag())
        encoded = self.encode()
        sender.stream(encoded)
        sender.advance()
        if sender.snd_settle_mode == Link.SND_SETTLED:
            dlv.settle()
        return dlv

    def recv(self, link):
        """
        Receives and decodes the message content for the current delivery
        from the link. Upon success it will return the current delivery
        for the link. If there is no current delivery, or if the current
        delivery is incomplete, or if the link is not a receiver, it will
        return None.

        @type link: Link
        @param link: the link to receive a message from
        @return the delivery associated with the decoded message (or None)

        """
        if link.is_sender: return None
        dlv = link.current
        if not dlv or dlv.partial: return None
        dlv.encoded = link.recv(dlv.pending)
        link.advance()
        # the sender has already forgotten about the delivery, so we might
        # as well too
        if link.remote_snd_settle_mode == Link.SND_SETTLED:
            dlv.settle()
        self.decode(dlv.encoded)
        return dlv

    def __repr2__(self):
        props = []
        for attr in ("inferred", "address", "reply_to", "durable", "ttl",
                     "priority", "first_acquirer", "delivery_count", "id",
                     "correlation_id", "user_id", "group_id", "group_sequence",
                     "reply_to_group_id", "instructions", "annotations",
                     "properties", "body"):
            value = getattr(self, attr)
            if value: props.append("%s=%r" % (attr, value))
        return "Message(%s)" % ", ".join(props)

    def __repr__(self):
        tmp = pn_string(None)
        err = pn_inspect(self._msg, tmp)
        result = pn_string_get(tmp)
        pn_free(tmp)
        self._check(err)
        return result
