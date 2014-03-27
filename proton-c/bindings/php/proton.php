<?php

/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

include("cproton.php");

class ProtonException extends Exception {}

class Timeout extends ProtonException {}

class MessengerException extends ProtonException {}

class MessageException extends ProtonException {}

function code2exc($err) {
  switch ($err) {
  case PN_TIMEOUT:
    return "Timeout";
  default:
    return null;
  }
}

class Messenger
{
  private $impl;

  public function __construct($name=null) {
    $this->impl = pn_messenger($name);
  }

  public function __destruct() {
    pn_messenger_free($this->impl);
  }

  public function __toString() {
    return 'Messenger("' . pn_messenger_name($this->impl) . '")';
  }

  private function _check($value) {
    if ($value < 0) {
      $exc = code2exc($value);
      if ($exc == null) $exc = "MessengerException";
      throw new $exc("[$value]: " . pn_error_text(pn_messenger_error($this->impl)));
    } else {
      return $value;
    }
  }

  public function __get($name) {
    switch ($name) {
    case "name":
      return pn_messenger_name($this->impl);
    case "certificate":
      return pn_messenger_get_certificate($this->impl);
    case "private_key":
      return pn_messenger_get_private_key($this->impl);
    case "password":
      return pn_messenger_get_password($this->impl);
    case "trusted_certificates":
      return pn_messenger_get_trusted_certificates($this->impl);
    case "incoming":
      return $this->incoming();
    case "outgoing":
      return $this->outgoing();
    default:
      throw new Exception("unknown property: " . $name);
    }
  }

  public function __set($name, $value) {
    switch ($name) {
    case "certificate":
      $this->_check(pn_messenger_set_certificate($this->impl, $value));
      break;
    case "private_key":
      $this->_check(pn_messenger_set_private_key($this->impl, $value));
      break;
    case "password":
      $this->_check(pn_messenger_set_password($this->impl, $value));
      break;
    case "trusted_certificates":
      $this->_check(pn_messenger_set_trusted_certificates($this->impl, $value));
      break;
    case "timeout":
      $this->_check(pn_messenger_set_timeout($this->impl, $value));
      break;
    case "outgoing_window":
      $this->_check(pn_messenger_set_outgoing_window($this->impl, $value));
      break;
    case "incoming_window":
      $this->_check(pn_messenger_set_incoming_window($this->impl, $value));
      break;
    default:
      throw new Exception("unknown property: " . $name);
    }
  }

  public function start() {
    $this->_check(pn_messenger_start($this->impl));
  }

  public function stop() {
    $this->_check(pn_messenger_stop($this->impl));
  }

  public function subscribe($source) {
    if ($source == null) {
      throw new MessengerException("null source passed to subscribe");
    }
    $this->_check(pn_messenger_subscribe($this->impl, $source));
  }

  public function outgoing_tracker() {
    return pn_messenger_outgoing_tracker($this->impl);
  }

  public function put($message) {
    $message->_pre_encode();
    $this->_check(pn_messenger_put($this->impl, $message->impl));
    return $this->outgoing_tracker();
  }

  public function send($n = -1) {
    $this->_check(pn_messenger_send($this->impl, $n));
  }

  public function recv($n = -1) {
    $this->_check(pn_messenger_recv($this->impl, $n));
  }

  public function incoming_tracker() {
    return pn_messenger_incoming_tracker($this->impl);
  }

  public function get($message) {
    $this->_check(pn_messenger_get($this->impl, $message->impl));
    $message->_post_decode();
    return $this->incoming_tracker();
  }

  public function accept($tracker = null) {
    if ($tracker == null) {
      $tracker = $this->incoming_tracker();
      $flag = PN_CUMULATIVE;
    } else {
      $flag = 0;
    }
    $this->_check(pn_messenger_accept($this->impl, $tracker, $flag));
  }

  public function reject($tracker = null) {
    if ($tracker == null) {
      $tracker = $this->incoming_tracker();
      $flag = PN_CUMULATIVE;
    } else {
      $flag = 0;
    }
    $this->_check(pn_messenger_reject($this->impl, $tracker, $flag));
  }

  public function route($pattern, $address) {
    $this->_check(pn_messenger_route($this->impl, $pattern, $address));
  }

  public function outgoing() {
    return pn_messenger_outgoing($this->impl);
  }

  public function incoming() {
    return pn_messenger_incoming($this->impl);
  }

  public function status($tracker) {
    return pn_messenger_status($this->impl, $tracker);
  }

}

class Message {

  const DATA = PN_DATA;
  const TEXT = PN_TEXT;
  const AMQP = PN_AMQP;
  const JSON = PN_JSON;

  const DEFAULT_PRIORITY = PN_DEFAULT_PRIORITY;

  var $impl;
  var $_id;
  var $_correlation_id;
  public $instructions = null;
  public $annotations = null;
  public $properties = null;
  public $body = null;

  public function __construct() {
    $this->impl = pn_message();
    $this->_id = new Data(pn_message_id($this->impl));
    $this->_correlation_id = new Data(pn_message_correlation_id($this->impl));
  }

  public function __destruct() {
    pn_message_free($this->impl);
  }

  public function __tostring() {
    $tmp = pn_string("");
    pn_inspect($this->impl, $tmp);
    $result = pn_string_get($tmp);
    pn_free($tmp);
    return $result;
  }

  private function _check($value) {
    if ($value < 0) {
      $exc = code2exc($value);
      if ($exc == null) $exc = "MessageException";
      throw new $exc("[$value]: " . pn_message_error($this->impl));
    } else {
      return $value;
    }
  }

  public function __get($name) {
    if ($name == "impl")
      throw new Exception();
    $getter = "_get_$name";
    return $this->$getter();
  }

  public function __set($name, $value) {
    $setter = "_set_$name";
    $this->$setter($value);
  }

  function _pre_encode() {
    $inst = new Data(pn_message_instructions($this->impl));
    $ann = new Data(pn_message_annotations($this->impl));
    $props = new Data(pn_message_properties($this->impl));
    $body = new Data(pn_message_body($this->impl));

    $inst->clear();
    if ($this->instructions != null)
      $inst->put_object($this->instructions);
    $ann->clear();
    if ($this->annotations != null)
      $ann->put_object($this->annotations);
    $props->clear();
    if ($this->properties != null)
      $props->put_object($this->properties);
    if ($this->body != null) {
      // XXX: move this out when load/save are gone
      $body->clear();
      $body->put_object($this->body);
    }
  }

  function _post_decode() {
    $inst = new Data(pn_message_instructions($this->impl));
    $ann = new Data(pn_message_annotations($this->impl));
    $props = new Data(pn_message_properties($this->impl));
    $body = new Data(pn_message_body($this->impl));

    if ($inst->next())
      $this->instructions = $inst->get_object();
    else
      $this->instructions = null;
    if ($ann->next())
      $this->annotations = $ann->get_object();
    else
      $self->annotations = null;
    if ($props->next())
      $this->properties = $props->get_object();
    else
      $this->properties = null;
    if ($body->next())
      $this->body = $body->get_object();
    else
      $this->body = null;
  }

  public function clear() {
    pn_message_clear($this->impl);
    $this->instructions = null;
    $this->annotations = null;
    $this->properties = null;
    $this->body = null;
  }

  private function _get_inferred() {
    return pn_message_is_inferred($this->impl);
  }

  private function _set_inferred($value) {
    $this->_check(pn_message_set_inferred($this->impl, $value));
  }

  private function _get_durable() {
    return pn_message_is_durable($this->impl);
  }

  private function _set_durable($value) {
    $this->_check(pn_message_set_durable($this->impl, $value));
  }

  private function _get_priority() {
    return pn_message_get_priority($this->impl);
  }

  private function _set_priority($value) {
    $this->_check(pn_message_set_priority($this->impl, $value));
  }

  private function _get_ttl() {
    return pn_message_get_ttl($this->impl);
  }

  private function _set_ttl($value) {
    $this->_check(pn_message_set_ttl($this->impl, $value));
  }

  private function _get_first_acquirer() {
    return pn_message_is_first_acquirer($this->impl);
  }

  private function _set_first_acquirer($value) {
    $this->_check(pn_message_set_first_acquirer($this->impl, $value));
  }

  private function _get_delivery_count() {
    return pn_message_get_delivery_count($this->impl);
  }

  private function _set_delivery_count($value) {
    $this->_check(pn_message_set_delivery_count($this->impl, $value));
  }

  private function _get_id() {
    return $this->_id->get_object();
  }

  private function _set_id($value) {
    $this->_id->rewind();
    $this->_id->put_object($value);
  }

  private function _get_user_id() {
    return pn_message_get_user_id($this->impl);
  }

  private function _set_user_id($value) {
    $this->_check(pn_message_set_user_id($this->impl, $value));
  }

  private function _get_address() {
    return pn_message_get_address($this->impl);
  }

  private function _set_address($value) {
    $this->_check(pn_message_set_address($this->impl, $value));
  }

  private function _get_subject() {
    return pn_message_get_subject($this->impl);
  }

  private function _set_subject($value) {
    $this->_check(pn_message_set_subject($this->impl, $value));
  }

  private function _get_reply_to() {
    return pn_message_get_reply_to($this->impl);
  }

  private function _set_reply_to($value) {
    $this->_check(pn_message_set_reply_to($this->impl, $value));
  }

  private function _get_correlation_id() {
    return $this->_correlation_id->get_object();
  }

  private function _set_correlation_id($value) {
    $this->_correlation_id->rewind();
    $this->_correlation_id->put_object($value);
  }

  private function _get_content_type() {
    return pn_message_get_content_type($this->impl);
  }

  private function _set_content_type($value) {
    $this->_check(pn_message_set_content_type($this->impl, $value));
  }

  private function _get_content_encoding() {
    return pn_message_get_content_encoding($this->impl);
  }

  private function _set_content_encoding($value) {
    $this->_check(pn_message_set_content_encoding($this->impl, $value));
  }

  private function _get_expiry_time() {
    return pn_message_get_expiry_time($this->impl);
  }

  private function _set_expiry_time($value) {
    $this->_check(pn_message_set_expiry_time($this->impl, $value));
  }

  private function _get_creation_time() {
    return pn_message_get_creation_time($this->impl);
  }

  private function _set_creation_time($value) {
    $this->_check(pn_message_set_creation_time($this->impl, $value));
  }

  private function _get_group_id() {
    return pn_message_get_group_id($this->impl);
  }

  private function _set_group_id($value) {
    $this->_check(pn_message_set_group_id($this->impl, $value));
  }

  private function _get_group_sequence() {
    return pn_message_get_group_sequence($this->impl);
  }

  private function _set_group_sequence($value) {
    $this->_check(pn_message_set_group_sequence($this->impl, $value));
  }

  private function _get_reply_to_group_id() {
    return pn_message_get_reply_to_group_id($this->impl);
  }

  private function _set_reply_to_group_id($value) {
    $this->_check(pn_message_set_reply_to_group_id($this->impl, $value));
  }

  # XXX
  private function _get_format() {
    return pn_message_get_format($this->impl);
  }

  private function _set_format($value) {
    $this->_check(pn_message_set_format($this->impl, $value));
  }

  public function encode() {
    $this->_pre_encode();
    $sz = 16;
    while (true) {
      list($err, $data) = pn_message_encode($this->impl, $sz);
      if ($err == PN_OVERFLOW) {
        $sz *= 2;
        continue;
      } else {
        $this->_check($err);
        return $data;
      }
    }
  }

  public function decode($data) {
    $this->_check(pn_message_decode($this->impl, $data, strlen($data)));
    $this->_post_decode();
  }

  public function load($data) {
    $this->_check(pn_message_load($this->impl, $data, strlen($data)));
  }

  public function save() {
    $sz = 16;
    while (true) {
      list($err, $data) = pn_message_save($this->impl, $sz);
      if ($err == PN_OVERFLOW) {
        $sz *= 2;
        continue;
      } else {
        $this->_check($err);
        return $data;
      }
    }
  }
}

class Binary {

  public $bytes;

  public function __construct($bytes) {
    $this->bytes = $bytes;
  }

  public function __tostring() {
    return "Binary($this->bytes)";
  }

}

class Symbol {

  public $name;

  public function __construct($name) {
    $this->name = $name;
  }

  public function __tostring() {
    return "Symbol($this->name)";
  }

}

class UUID {

  public $bytes;

  public function __construct($bytes) {
    if (strlen($bytes) != 16) {
      throw new Exception("invalid argument: exactly 16 bytes required");
    }
    $this->bytes = $bytes;
  }

  public function __tostring() {
    $b = $this->bytes;
    return sprintf("UUID(%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
                   ord($b[0]), ord($b[1]), ord($b[2]), ord($b[3]),
                   ord($b[4]), ord($b[5]), ord($b[6]), ord($b[7]), ord($b[8]), ord($b[9]),
                   ord($b[10]), ord($b[11]), ord($b[12]), ord($b[13]), ord($b[14]), ord($b[15]));
  }

}

class PList {

  public $elements;

  public function __construct() {
    $this->elements = func_get_args();
  }

  public function __tostring() {
    return "PList(" . implode(", ", $this->elements) . ")";
  }

}

class Char {

  public $codepoint;

  public function __construct($codepoint) {
    $this->codepoint = $codepoint;
  }

  public function __tostring() {
    return "Char($this->codepoint)";
  }

}

class Described {

  public $descriptor;
  public $value;

  public function __construct($descriptor, $value) {
    $this->descriptor = $descriptor;
    $this->value = $value;
  }

  public function __tostring() {
    return "Described($this->descriptor, $this->value)";
  }

}

class DataException extends ProtonException {}

class Data {

  const NULL = PN_NULL;
  const BOOL = PN_BOOL;
  const UBYTE = PN_UBYTE;
  const BYTE = PN_BYTE;
  const USHORT = PN_USHORT;
  const SHORT = PN_SHORT;
  const UINT = PN_UINT;
  const INT = PN_INT;
  const CHAR = PN_CHAR;
  const ULONG = PN_ULONG;
  const LONG = PN_LONG;
  const TIMESTAMP = PN_TIMESTAMP;
  const FLOAT = PN_FLOAT;
  const DOUBLE = PN_DOUBLE;
  const DECIMAL32 = PN_DECIMAL32;
  const DECIMAL64 = PN_DECIMAL64;
  const DECIMAL128 = PN_DECIMAL128;
  const UUID = PN_UUID;
  const BINARY = PN_BINARY;
  const STRING = PN_STRING;
  const SYMBOL = PN_SYMBOL;
  const DESCRIBED = PN_DESCRIBED;
  const PARRAY = PN_ARRAY;
  const PLIST = PN_LIST;
  const MAP = PN_MAP;

  private $impl;
  private $free;

  public function __construct($capacity=16) {
    if (is_int($capacity)) {
      $this->impl = pn_data($capacity);
      $this->free = true;
    } else {
      $this->impl = $capacity;
      $this->free = false;
    }
  }

  public function __destruct() {
    if ($this->free)
      pn_data_free($this->impl);
  }

  public function _check($value) {
    if ($value < 0) {
      $exc = code2exc($value);
      if ($exc == null) $exc = "DataException";
      throw new $exc("[$value]");
    } else {
      return $value;
    }
  }

  public function clear() {
    pn_data_clear($this->impl);
  }

  public function rewind() {
    pn_data_rewind($this->impl);
  }

  public function next() {
    $found = pn_data_next($this->impl);
    if ($found)
      return $this->type();
    else
      return null;
  }

  public function prev() {
    $found = pn_data_prev($this->impl);
    if ($found)
      return $this->type();
    else
      return null;
  }

  public function enter() {
    return pn_data_enter($this->impl);
  }

  public function exit_() {
    return pn_data_exit($this->impl);
  }

  public function type() {
    $dtype = pn_data_type($this->impl);
    if ($dtype == -1)
      return null;
    else
      return $dtype;
  }

  public function encode() {
    $size = 1024;
    while (true) {
      list($cd, $enc) = pn_data_encode($this->impl, $size);
      if ($cd == PN_OVERFLOW)
        $size *= 2;
      else if ($cd >= 0)
        return $enc;
      else
        $this->_check($cd);
    }
  }

  public function decode($encoded) {
    return $this->_check(pn_data_decode($this->impl, $encoded));
  }

  public function put_list() {
    $this->_check(pn_data_put_list($this->impl));
  }

  public function put_map() {
    $this->_check(pn_data_put_map($this->impl));
  }

  public function put_array($described, $element_type) {
    $this->_check(pn_data_put_array($this->impl, $described, $element_type));
  }

  public function put_described() {
    $this->_check(pn_data_put_described($this->impl));
  }

  public function put_null() {
    $this->_check(pn_data_put_null($this->impl));
  }

  public function put_bool($b) {
    $this->_check(pn_data_put_bool($this->impl, $b));
  }

  public function put_ubyte($ub) {
    $this->_check(pn_data_put_ubyte($this->impl, $ub));
  }

  public function put_byte($b) {
    $this->_check(pn_data_put_byte($this->impl, $b));
  }

  public function put_ushort($us) {
    $this->_check(pn_data_put_ushort($this->impl, $us));
  }

  public function put_short($s) {
    $this->_check(pn_data_put_short($this->impl, $s));
  }

  public function put_uint($ui) {
    $this->_check(pn_data_put_uint($this->impl, $ui));
  }

  public function put_int($i) {
    $this->_check(pn_data_put_int($this->impl, $i));
  }

  public function put_char($c) {
    if ($c instanceof Char) {
      $c = $c->codepoint;
    } else {
      $c = ord($c);
    }
    $this->_check(pn_data_put_char($this->impl, $c));
  }

  public function put_ulong($ul) {
    $this->_check(pn_data_put_ulong($this->impl, $ul));
  }

  public function put_long($l) {
    $this->_check(pn_data_put_long($this->impl, $l));
  }

  public function put_timestamp($t) {
    $this->_check(pn_data_put_timestamp($this->impl, $t));
  }

  public function put_float($f) {
    $this->_check(pn_data_put_float($this->impl, $f));
  }

  public function put_double($d) {
    $this->_check(pn_data_put_double($this->impl, $d));
  }

  public function put_decimal32($d) {
    $this->_check(pn_data_put_decimal32($this->impl, $d));
  }

  public function put_decimal64($d) {
    $this->_check(pn_data_put_decimal64($this->impl, $d));
  }

  public function put_decimal128($d) {
    $this->_check(pn_data_put_decimal128($this->impl, $d));
  }

  public function put_uuid($u) {
    if ($u instanceof UUID) {
      $u = $u->bytes;
    }
    $this->_check(pn_data_put_uuid($this->impl, $u));
  }

  public function put_binary($b) {
    if ($b instanceof Binary) {
      $b = $b->bytes;
    }
    $this->_check(pn_data_put_binary($this->impl, $b));
  }

  public function put_string($s) {
    $this->_check(pn_data_put_string($this->impl, $s));
  }

  public function put_symbol($s) {
    if ($s instanceof Symbol) {
      $s = $s->name;
    }
    $this->_check(pn_data_put_symbol($this->impl, $s));
  }

  public function get_list() {
    return pn_data_get_list($this->impl);
  }

  public function get_map() {
    return pn_data_get_map($this->impl);
  }

  public function get_array() {
    $count = pn_data_get_array($this->impl);
    $described = pn_data_is_array_described($this->impl);
    $type = pn_data_get_array_type($this->impl);
    if ($type == -1)
      $type = null;
    return array($count, $described, $type);
  }

  public function is_described() {
    return pn_data_is_described($this->impl);
  }

  public function is_null() {
    $this->_check(pn_data_get_null($this->impl));
  }

  public function get_bool() {
    return pn_data_get_bool($this->impl);
  }

  public function get_ubyte() {
    return pn_data_get_ubyte($this->impl);
  }

  public function get_byte() {
    return pn_data_get_byte($this->impl);
  }

  public function get_ushort() {
    return pn_data_get_ushort($this->impl);
  }

  public function get_short() {
    return pn_data_get_short($this->impl);
  }

  public function get_uint() {
    return pn_data_get_uint($this->impl);
  }

  public function get_int() {
    return pn_data_get_int($this->impl);
  }

  public function get_char() {
    return new Char(pn_data_get_char($this->impl));
  }

  public function get_ulong() {
    return pn_data_get_ulong($this->impl);
  }

  public function get_long() {
    return pn_data_get_long($this->impl);
  }

  public function get_timestamp() {
    return pn_data_get_timestamp($this->impl);
  }

  public function get_float() {
    return pn_data_get_float($this->impl);
  }

  public function get_double() {
    return pn_data_get_double($this->impl);
  }

  # XXX: need to convert
  public function get_decimal32() {
    return pn_data_get_decimal32($this->impl);
  }

  # XXX: need to convert
  public function get_decimal64() {
    return pn_data_get_decimal64($this->impl);
  }

  # XXX: need to convert
  public function get_decimal128() {
    return pn_data_get_decimal128($this->impl);
  }

  public function get_uuid() {
    if (pn_data_type($this->impl) == Data::UUID)
      return new UUID(pn_data_get_uuid($this->impl));
    else
      return null;
  }

  public function get_binary() {
    return new Binary(pn_data_get_binary($this->impl));
  }

  public function get_string() {
    return pn_data_get_string($this->impl);
  }

  public function get_symbol() {
    return new Symbol(pn_data_get_symbol($this->impl));
  }

  public function copy($src) {
    $this->_check(pn_data_copy($this->impl, $src->impl));
  }

  public function format() {
    $sz = 16;
    while (true) {
      list($err, $result) = pn_data_format($this->impl, $sz);
      if ($err == PN_OVERFLOW) {
        $sz *= 2;
        continue;
      } else {
        $this->_check($err);
        return $result;
      }
    }
  }

  public function dump() {
    pn_data_dump($this->impl);
  }

  public function get_null() {
    return null;
  }

  public function get_php_described() {
    if ($this->enter()) {
      try {
        $this->next();
        $descriptor = $this->get_object();
        $this->next();
        $value = $this->get_object();
        $this->exit_();
      } catch (Exception $e) {
        $this->exit_();
        throw $e;
      }
      return new Described($descriptor, $value);
    }
  }

  public function get_php_array() {
    if ($this->enter()) {
      try {
        $result = array();
        while ($this->next()) {
          $result[] = $this->get_object();
        }
        $this->exit_();
      } catch (Exception $e) {
        $this->exit_();
        throw $e;
      }
      return $result;
    }
  }

  public function put_php_list($lst) {
    $this->put_list();
    $this->enter();
    try {
      foreach ($lst->elements as $e) {
        $this->put_object($e);
      }
      $this->exit_();
    } catch (Exception $e) {
      $this->exit_();
      throw $e;
    }
  }

  public function get_php_list() {
    if ($this->enter()) {
      try {
        $result = new PList();
        while ($this->next()) {
          $result->elements[] = $this->get_object();
        }
        $this->exit_();
      } catch (Exception $e) {
        $this->exit_();
        throw $e;
      }

      return $result;
    }
  }

  public function put_php_map($ary) {
    $this->put_map();
    $this->enter();
    try {
      foreach ($ary as $k => $v) {
        $this->put_object($k);
        $this->put_object($v);
      }
      $this->exit_();
    } catch (Exception $e) {
      $this->exit_();
      throw $e;
    }
  }

  public function get_php_map() {
    if ($this->enter()) {
      try {
        $result = array();
        while ($this->next()) {
          $k = $this->get_object();
          switch ($this->type()) {
          case Data::BINARY:
            $k = $k->bytes;
            break;
          case Data::SYMBOL:
            $k = $k->name;
            break;
          case Data::STRING:
          case Data::UBYTE:
          case Data::BYTE:
          case Data::USHORT:
          case Data::SHORT:
          case Data::UINT:
          case Data::INT:
          case Data::ULONG:
          case Data::LONG:
            break;
          default:
            $k = "$k";
            break;
          }
          if ($this->next())
            $v = $this->get_object();
          else
            $v = null;
          $result[$k] = $v;
        }
        $this->exit_();
      } catch (Exception $e) {
        $this->exit_();
        throw $e;
      }
      return $result;
    }
  }

  private $put_mappings = array
    ("NULL" => "put_null",
     "boolean" => "put_bool",
     "UUID" => "put_uuid",
     "string" => "put_string",
     "Binary" => "put_binary",
     "Symbol" => "put_symbol",
     "integer" => "put_long",
     "Char" => "put_char",
     "double" => "put_double",
     "Described" => "put_php_described",
     "PList" => "put_php_list",
     "array" => "put_php_map"
     );
  private $get_mappings = array
    (Data::NULL => "get_null",
     Data::BOOL => "get_bool",
     Data::UBYTE => "get_ubyte",
     Data::BYTE => "get_byte",
     Data::USHORT => "get_ushort",
     Data::SHORT => "get_short",
     Data::UINT => "get_uint",
     Data::INT => "get_int",
     Data::CHAR => "get_char",
     Data::ULONG => "get_ulong",
     Data::LONG => "get_long",
     Data::TIMESTAMP => "get_timestamp",
     Data::FLOAT => "get_float",
     Data::DOUBLE => "get_double",
     Data::DECIMAL32 => "get_decimal32",
     Data::DECIMAL64 => "get_decimal64",
     Data::DECIMAL128 => "get_decimal128",
     Data::UUID => "get_uuid",
     Data::BINARY => "get_binary",
     Data::STRING => "get_string",
     Data::SYMBOL => "get_symbol",
     Data::DESCRIBED => "get_php_described",
     Data::PARRAY => "get_php_array",
     Data::PLIST => "get_php_list",
     Data::MAP => "get_php_map"
     );

  public function put_object($obj) {
    $type = gettype($obj);
    if ($type == "object") {
      $type = get_class($obj);
    }
    $putter = $this->put_mappings[$type];
    if ($putter == null)
      throw new DataException("unknown type: $type");
    $this->$putter($obj);
  }

  public function get_object() {
    $type = $this->type();
    if ($type == null) return null;
    $getter = $this->get_mappings[$type];
    return $this->$getter();
  }

}

?>
