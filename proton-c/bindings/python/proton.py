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

  def get_certificate(self):
    return pn_messenger_get_certificate(self._mng)

  def set_certificate(self, value):
    self._check(pn_messenger_set_certificate(self._mng, value))

  certificate = property(get_certificate, set_certificate)

  def get_private_key(self):
    return pn_messenger_get_private_key(self._mng)

  def set_private_key(self, value):
    self._check(pn_messenger_set_private_key(self._mng, value))

  private_key = property(get_private_key, set_private_key)

  def get_password(self):
    return pn_messenger_get_password(self._mng)

  def set_password(self, value):
    self._check(pn_messenger_set_password(self._mng, value))

  password = property(get_password, set_password)

  def get_trusted_certificates(self):
    return pn_messenger_get_trusted_certificates(self._mng)

  def set_trusted_certificates(self, value):
    self._check(pn_messenger_set_trusted_certificates(self._mng, value))

  trusted_certificates = property(get_trusted_certificates, set_trusted_certificates)

  def get_timeout(self):
    return pn_messenger_get_timeout(self._mng)

  def set_timeout(self, value):
    self._check(pn_messenger_set_timeout(self._mng, value))

  timeout = property(get_timeout, set_timeout)

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

  def is_durable(self):
    return pn_message_is_durable(self._msg)

  def set_durable(self, value):
    self._check(pn_message_set_durable(self._msg, bool(value)))

  durable = property(is_durable, set_durable)

  def get_priority(self):
    return pn_message_get_priority(self._msg)

  def set_priority(self, value):
    self._check(pn_message_set_priority(self._msg, value))

  priority = property(get_priority, set_priority)

  def get_ttl(self):
    return pn_message_get_ttl(self._msg)

  def set_ttl(self, value):
    self._check(pn_message_set_ttl(self._msg, value))

  ttl = property(get_ttl, set_ttl)

  def is_first_acquirer(self):
    return pn_message_is_first_acquirer(self._msg)

  def set_first_acquirer(self, value):
    self._check(pn_message_set_first_acquirer(self._msg, bool(value)))

  first_acquirer = property(is_first_acquirer, set_first_acquirer)

  def get_delivery_count(self):
    return pn_message_get_delivery_count(self._msg)

  def set_delivery_count(self, value):
    self._check(pn_message_set_delivery_count(self._msg, value))

  delivery_count = property(get_delivery_count, set_delivery_count)

  # XXX
  def get_id(self):
    return pn_message_get_id(self._msg)

  def set_id(self, value):
    self._check(pn_message_set_id(self._msg, value))

  id = property(get_id, set_id)

  def get_user_id(self):
    return pn_message_get_user_id(self._msg)

  def set_user_id(self, value):
    self._check(pn_message_set_user_id(self._msg, value))

  user_id = property(get_user_id, set_user_id)

  def get_address(self):
    return pn_message_get_address(self._msg)

  def set_address(self, value):
    self._check(pn_message_set_address(self._msg, value))

  address = property(get_address, set_address)

  def get_subject(self):
    return pn_message_get_subject(self._msg)

  def set_subject(self, value):
    self._check(pn_message_set_subject(self._msg, value))

  subject = property(get_subject, set_subject)

  def get_reply_to(self):
    return pn_message_get_reply_to(self._msg)

  def set_reply_to(self, value):
    self._check(pn_message_set_reply_to(self._msg, value))

  reply_to = property(get_reply_to, set_reply_to)

  # XXX
  def get_correlation_id(self):
    return pn_message_get_correlation_id(self._msg)

  def set_correlation_id(self, value):
    self._check(pn_message_set_correlation_id(self._msg, value))

  correlation_id = property(get_correlation_id, set_correlation_id)

  def get_content_type(self):
    return pn_message_get_content_type(self._msg)

  def set_content_type(self, value):
    self._check(pn_message_set_content_type(self._msg, value))

  content_type = property(get_content_type, set_content_type)

  def get_content_encoding(self):
    return pn_message_get_content_encoding(self._msg)

  def set_content_encoding(self, value):
    self._check(pn_message_set_content_encoding(self._msg, value))

  content_encoding = property(get_content_encoding, set_content_encoding)

  def get_expiry_time(self):
    return pn_message_get_expiry_time(self._msg)

  def set_expiry_time(self, value):
    self._check(pn_message_set_expiry_time(self._msg, value))

  expiry_time = property(get_expiry_time, set_expiry_time)

  def get_creation_time(self):
    return pn_message_get_creation_time(self._msg)

  def set_creation_time(self, value):
    self._check(pn_message_set_creation_time(self._msg, value))

  creation_time = property(get_creation_time, set_creation_time)

  def get_group_id(self):
    return pn_message_get_group_id(self._msg)

  def set_group_id(self, value):
    self._check(pn_message_set_group_id(self._msg, value))

  group_id = property(get_group_id, set_group_id)

  def get_group_sequence(self):
    return pn_message_get_group_sequence(self._msg)

  def set_group_sequence(self, value):
    self._check(pn_message_set_group_sequence(self._msg, value))

  group_sequence = property(get_group_sequence, set_group_sequence)

  def get_reply_to_group_id(self):
    return pn_message_get_reply_to_group_id(self._msg)

  def set_reply_to_group_id(self, value):
    self._check(pn_message_set_reply_to_group_id(self._msg, value))

  reply_to_group_id = property(get_reply_to_group_id, set_reply_to_group_id)

  # XXX
  def get_format(self):
    return pn_message_get_format(self._msg)

  def set_format(self, value):
    self._check(pn_message_set_format(self._msg, value))

  format = property(get_format, set_format)

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
