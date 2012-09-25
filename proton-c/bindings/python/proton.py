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
    self._mng = pn_messenger(name)

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
  def certificate(self):
    return pn_messenger_get_certificate(self._mng)

  @certificate.setter
  def certificate(self, value):
    self._check(pn_messenger_set_certificate(self._mng, value))

  @property
  def private_key(self):
    return pn_messenger_get_private_key(self._mng)

  @private_key.setter
  def private_key(self, value):
    self._check(pn_messenger_set_private_key(self._mng, value))

  @property
  def password(self):
    return pn_messenger_get_password(self._mng)

  @password.setter
  def password(self, value):
    self._check(pn_messenger_set_password(self._mng, value))

  @property
  def trusted_certificates(self):
    return pn_messenger_get_trusted_certificates(self._mng)

  @trusted_certificates.setter
  def trusted_certificates(self, value):
    self._check(pn_messenger_set_trusted_certificates(self._mng, value))

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

class DataException(ProtonException):
  pass

class Data:

  NULL = PN_NULL
  BOOL = PN_BOOL
  UBYTE = PN_UBYTE
  BYTE = PN_BYTE
  USHORT = PN_USHORT
  SHORT = PN_SHORT
  UINT = PN_UINT
  INT = PN_INT
  ULONG = PN_ULONG
  LONG = PN_LONG
  FLOAT = PN_FLOAT
  DOUBLE = PN_DOUBLE
  BINARY = PN_BINARY
  STRING = PN_STRING
  SYMBOL = PN_SYMBOL
  DESCRIBED = PN_DESCRIPTOR
  ARRAY = PN_ARRAY
  LIST = PN_LIST
  MAP = PN_MAP

  def __init__(self, capacity=16):
    self._data = pn_data(capacity)

  def __del__(self):
    if hasattr(self, "_data"):
      pn_data_free(self._data)
      del self._data

  def _check(self, err):
    if err < 0:
      exc = EXCEPTIONS.get(err, DataException)
      raise exc("[%s]: %s" % (err, "xxx"))
    else:
      return err

  def rewind(self):
    pn_data_rewind(self._data)

  def next(self):
    found, dtype = pn_data_next(self._data)
    if found:
      return dtype
    else:
      return None

  def prev(self):
    found, dtype = pn_data_prev(self._data)
    if found:
      return dtype
    else:
      return None

  def enter(self):
    return pn_data_enter(self._data)

  def exit(self):
    return pn_data_exit(self._data)

  def encode(self):
    size = 1024
    while True:
      cd, enc = pn_data_encode(self._data, size)
      if cd == PN_OVERFLOW:
        size *= 2
      elif cd >= 0:
        return enc
      else:
        self._check(cd)

  def decode(self, encoded):
    return self._check(pn_data_decode(self._data, encoded))

  def put_list(self):
    self._check(pn_data_put_list(self._data))

  def put_map(self):
    self._check(pn_data_put_map(self._data))

  def put_array(self, described, etype):
    self._check(pn_data_put_array(self._data, described, etype))

  def put_described(self):
    self._check(pn_data_put_described(self._data))

  def put_null(self):
    self._check(pn_data_put_null(self._data))

  def put_bool(self, b):
    self._check(pn_data_put_bool(self._data, b))

  def put_ubyte(self, ub):
    self._check(pn_data_put_ubyte(self._data, ub))

  def put_byte(self, b):
    self._check(pn_data_put_byte(self._data, b))

  def put_ushort(self, us):
    self._check(pn_data_put_ushort(self._data, us))

  def put_short(self, s):
    self._check(pn_data_put_short(self._data, s))

  def put_uint(self, ui):
    self._check(pn_data_put_uint(self._data, ui))

  def put_int(self, i):
    self._check(pn_data_put_int(self._data, i))

  def put_ulong(self, ul):
    self._check(pn_data_put_ulong(self._data, ul))

  def put_long(self, l):
    self._check(pn_data_put_long(self._data, l))

  def put_float(self, f):
    self._check(pn_data_put_float(self._data, f))

  def put_double(self, d):
    self._check(pn_data_put_double(self._data, d))

  def put_binary(self, b):
    self._check(pn_data_put_binary(self._data, b))

  def put_string(self, s):
    self._check(pn_data_put_string(self._data, s))

  def put_symbol(self, s):
    self._check(pn_data_put_symbol(self._data, s))

  def get_list(self):
    err, count = pn_data_get_list(self._data)
    self._check(err)
    return count

  def get_map(self):
    err, count = pn_data_get_map(self._data)
    self._check(err)
    return count

  def get_array(self):
    err, count, described, type = pn_data_get_array(self._data)
    self._check(err)
    return count, described, type

  def get_described(self):
    self._check(pn_data_get_described(self._data))

  def get_null(self):
    self._check(pn_data_get_null(self._data))

  def get_bool(self):
    err, b = pn_data_get_bool(self._data)
    self._check(err)
    return b

  def get_ubyte(self):
    err, value = pn_data_get_ubyte(self._data)
    self._check(err)
    return value

  def get_byte(self):
    err, value = pn_data_get_byte(self._data)
    self._check(err)
    return value

  def get_ushort(self):
    err, value = pn_data_get_ushort(self._data)
    self._check(err)
    return value

  def get_short(self):
    err, value = pn_data_get_short(self._data)
    self._check(err)
    return value

  def get_uint(self):
    err, value = pn_data_get_uint(self._data)
    self._check(err)
    return value

  def get_int(self):
    err, value = pn_data_get_int(self._data)
    self._check(err)
    return value

  def get_ulong(self):
    err, value = pn_data_get_ulong(self._data)
    self._check(err)
    return value

  def get_long(self):
    err, value = pn_data_get_long(self._data)
    self._check(err)
    return value

  def get_float(self):
    err, value = pn_data_get_float(self._data)
    self._check(err)
    return value

  def get_double(self):
    err, value = pn_data_get_double(self._data)
    self._check(err)
    return value

  def get_binary(self):
    err, value = pn_data_get_binary(self._data)
    self._check(err)
    return value

  def get_string(self):
    err, value = pn_data_get_string(self._data)
    self._check(err)
    return value

  def get_symbol(self):
    err, value = pn_data_get_symbol(self._data)
    self._check(err)
    return value

  def dump(self):
    pn_data_dump(self._data)

__all__ = ["Messenger", "Message", "ProtonException", "MessengerException",
           "MessageException", "Timeout", "Data"]
