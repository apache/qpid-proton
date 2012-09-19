from xproton import *

class ProtonException(Exception):
  pass

class Timeout(ProtonException):
  pass

class MessengerException(ProtonException):
  pass

class MessageException(ProtonException):
  pass

EXCEPTIONS = {
  PN_TIMEOUT: Timeout
  }

class Messenger(object):

  def __init__(self, name=None):
    self._mng = pn_messenger(name);

  def __del__(self):
    if hasattr(self, "_mng"):
      pn_messenger_free(self._mng)
      del self._mng

  def _check(self, err):
    if err < 0:
      exc = EXCEPTIONS.get(err, MessengerException)
      raise exc("[%s]: %s" % (err, pn_messenger_error(self._mng)))
    else:
      return err

  @property
  def name(self):
    return pn_messenger_name(self._mng)

  @property
  def timeout(self):
    return pn_messenger_get_timeout(self._mng)

  @timeout.setter
  def timeout(self, value):
    self._check(pn_messenger_set_timeout(self._mng, value))

  def start(self):
    self._check(pn_messenger_start(self._mng))

  def stop(self):
    self._check(pn_messenger_stop(self._mng))

  def subscribe(self, source):
    self._check(pn_messenger_subscribe(self._mng, source))

  def put(self, msg):
    self._check(pn_messenger_put(self._mng, msg._msg))

  def send(self):
    self._check(pn_messenger_send(self._mng))

  def recv(self, n):
    self._check(pn_messenger_recv(self._mng, n))

  def get(self, msg):
    self._check(pn_messenger_get(self._mng, msg._msg))

  @property
  def outgoing(self):
    return pn_messenger_outgoing(self._mng)

  @property
  def incoming(self):
    return pn_messenger_incoming(self._mng)

class Message(object):

  DATA = PN_DATA
  TEXT = PN_TEXT
  AMQP = PN_AMQP
  JSON = PN_JSON

  def __init__(self):
    self._msg = pn_message()

  def __del__(self):
    if hasattr(self, "_msg"):
      pn_message_free(self._msg)
      del self._msg

  def _check(self, err):
    if err < 0:
      exc = EXCEPTIONS.get(err, MessageException)
      raise exc("[%s]: %s" % (err, pn_message_error(self._msg)))
    else:
      return err

  def clear(self):
    pn_message_clear(self._msg)

  @property
  def durable(self):
    return pn_message_is_durable(self._msg)

  @durable.setter
  def durable(self, value):
    self._check(pn_message_set_durable(self._msg, bool(value)))

  @property
  def priority(self):
    return pn_message_get_priority(self._msg)

  @priority.setter
  def priority(self, value):
    self._check(pn_message_set_priority(self._msg, value))

  @property
  def ttl(self):
    return pn_message_get_ttl(self._msg)

  @ttl.setter
  def ttl(self, value):
    self._check(pn_message_set_ttl(self._msg, value))

  @property
  def first_acquirer(self):
    return pn_message_is_first_acquirer(self._msg)

  @first_acquirer.setter
  def first_acquirer(self, value):
    self._check(pn_message_set_first_acquirer(self._msg, bool(value)))

  @property
  def delivery_count(self):
    return pn_message_get_delivery_count(self._msg)

  @delivery_count.setter
  def delivery_count(self, value):
    self._check(pn_message_set_delivery_count(self._msg, value))

  # XXX
  @property
  def id(self):
    return pn_message_get_id(self._msg)

  @id.setter
  def id(self, value):
    self._check(pn_message_set_id(self._msg, value))

  @property
  def user_id(self):
    return pn_message_get_user_id(self._msg)

  @user_id.setter
  def user_id(self, value):
    self._check(pn_message_set_user_id(self._msg, value))

  @property
  def address(self):
    return pn_message_get_address(self._msg)

  @address.setter
  def address(self, value):
    self._check(pn_message_set_address(self._msg, value))

  @property
  def subject(self):
    return pn_message_get_subject(self._msg)

  @subject.setter
  def subject(self, value):
    self._check(pn_message_set_subject(self._msg, value))

  @property
  def reply_to(self):
    return pn_message_get_reply_to(self._msg)

  @reply_to.setter
  def reply_to(self, value):
    self._check(pn_message_set_reply_to(self._msg, value))

  # XXX
  @property
  def correlation_id(self):
    return pn_message_get_correlation_id(self._msg)

  @correlation_id.setter
  def correlation_id(self, value):
    self._check(pn_message_set_correlation_id(self._msg, value))

  @property
  def content_type(self):
    return pn_message_get_content_type(self._msg)

  @content_type.setter
  def content_type(self, value):
    self._check(pn_message_set_content_type(self._msg, value))

  @property
  def content_encoding(self):
    return pn_message_get_content_encoding(self._msg)

  @content_encoding.setter
  def content_encoding(self, value):
    self._check(pn_message_set_content_encoding(self._msg, value))

  @property
  def expiry_time(self):
    return pn_message_get_expiry_time(self._msg)

  @expiry_time.setter
  def expiry_time(self, value):
    self._check(pn_message_set_expiry_time(self._msg, value))

  @property
  def creation_time(self):
    return pn_message_get_creation_time(self._msg)

  @creation_time.setter
  def creation_time(self, value):
    self._check(pn_message_set_creation_time(self._msg, value))

  @property
  def group_id(self):
    return pn_message_get_group_id(self._msg)

  @group_id.setter
  def group_id(self, value):
    self._check(pn_message_set_group_id(self._msg, value))

  @property
  def group_sequence(self):
    return pn_message_get_group_sequence(self._msg)

  @group_sequence.setter
  def group_sequence(self, value):
    self._check(pn_message_set_group_sequence(self._msg, value))

  @property
  def reply_to_group_id(self):
    return pn_message_get_reply_to_group_id(self._msg)

  @reply_to_group_id.setter
  def reply_to_group_id(self, value):
    self._check(pn_message_set_reply_to_group_id(self._msg, value))

  # XXX
  @property
  def format(self):
    return pn_message_get_format(self._msg)

  @format.setter
  def format(self, value):
    self._check(pn_message_set_format(self._msg, value))

  def load(self, data):
    self._check(pn_message_load(self._msg, data))

  def save(self):
    sz = 16
    while True:
      err, data = pn_message_save(self._msg, sz)
      if err == PN_OVERFLOW:
        sz *= 2
        continue
      else:
        self._check(err)
        return data

__all__ = ["Messenger", "Message", "ProtonException", "MessengerException",
           "MessageException", "Timeout"]
