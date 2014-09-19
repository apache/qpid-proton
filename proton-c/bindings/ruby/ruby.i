/*
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
 */
%module cproton

%{
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/sasl.h>
#include <proton/driver.h>
#include <proton/messenger.h>
#include <proton/ssl.h>
#include <proton/driver_extras.h>
#include <proton/types.h>
#include <proton/url.h>

#include <uuid/uuid.h>
%}

%include <cstring.i>

%cstring_output_withsize(char *OUTPUT, size_t *OUTPUT_SIZE)
%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));
%cstring_output_maxsize(char *OUTPUT, size_t MAX_OUTPUT_SIZE)

%{
#if !defined(RSTRING_LEN)
#  define RSTRING_LEN(x) (RSTRING(X)->len)
#  define RSTRING_PTR(x) (RSTRING(x)->ptr)
#endif
%}

%typemap(in) pn_bytes_t {
  if ($input == Qnil) {
    $1.start = NULL;
    $1.size = 0;
  } else {
    $1.start = RSTRING_PTR($input);
    if (!$1.start) {
      $1.size = 0;
    }
    $1.size = RSTRING_LEN($input);
  }
}

%typemap(out) pn_bytes_t {
  $result = rb_str_new($1.start, $1.size);
}

%typemap(in) pn_atom_t
{
  if ($input == Qnil)
    {
      $1.type = PN_NULL;
    }
  else
    {
      switch(TYPE($input))
        {
        case T_TRUE:
          $1.type = PN_BOOL;
          $1.u.as_bool = true;
          break;

        case T_FALSE:
          $1.type = PN_BOOL;
          $1.u.as_bool = false;
          break;

        case T_FLOAT:
          $1.type = PN_FLOAT;
          $1.u.as_float = NUM2DBL($input);
          break;

        case T_STRING:
          $1.type = PN_STRING;
          $1.u.as_bytes.start = RSTRING_PTR($input);
          if ($1.u.as_bytes.start)
            {
              $1.u.as_bytes.size = RSTRING_LEN($input);
            }
          else
            {
              $1.u.as_bytes.size = 0;
            }
          break;

        case T_FIXNUM:
          $1.type = PN_INT;
          $1.u.as_int = FIX2LONG($input);
          break;

        case T_BIGNUM:
          $1.type = PN_LONG;
          $1.u.as_long = NUM2LL($input);
          break;

        }
    }
}

%typemap(out) pn_atom_t
{
  switch($1.type)
    {
    case PN_NULL:
      $result = Qnil;
      break;

    case PN_BOOL:
      $result = $1.u.as_bool ? Qtrue : Qfalse;
      break;

    case PN_BYTE:
      $result = INT2NUM($1.u.as_byte);
      break;

    case PN_UBYTE:
      $result = UINT2NUM($1.u.as_ubyte);
      break;

    case PN_SHORT:
      $result = INT2NUM($1.u.as_short);
      break;

    case PN_USHORT:
      $result = UINT2NUM($1.u.as_ushort);
      break;

    case PN_INT:
      $result = INT2NUM($1.u.as_int);
      break;

     case PN_UINT:
      $result = UINT2NUM($1.u.as_uint);
      break;

    case PN_LONG:
      $result = LL2NUM($1.u.as_long);
      break;

    case PN_ULONG:
      $result = ULL2NUM($1.u.as_ulong);
      break;

    case PN_FLOAT:
      $result = rb_float_new($1.u.as_float);
      break;

    case PN_DOUBLE:
      $result = rb_float_new($1.u.as_double);
      break;

    case PN_STRING:
      $result = rb_str_new($1.u.as_bytes.start, $1.u.as_bytes.size);
      break;
    }
}

%typemap (in) pn_decimal32_t
{
  $1 = FIX2UINT($input);
}

%typemap (out) pn_decimal32_t
{
  $result = ULL2NUM($1);
}

%typemap (in) pn_decimal64_t
{
  $1 = NUM2ULONG($input);
}

%typemap (out) pn_decimal64_t
{
  $result = ULL2NUM($1);
}

%typemap (in) pn_decimal128_t
{
  int index;

  for(index = 0; index < 16; index++)
    {
      VALUE element = rb_ary_entry($input, index);
      $1.bytes[16 - (index + 1)] = FIX2INT(element);
    }
}

%typemap (out) pn_decimal128_t
{
  int index;

  $result = rb_ary_new2(16);
  for(index = 0; index < 16; index++)
    {
      rb_ary_store($result, 16 - (index + 1), CHR2FIX($1.bytes[index]));
    }
}

%typemap (in) pn_uuid_t
{
  int index;

  for(index = 0; index < 16; index++)
    {
      VALUE element = rb_ary_entry($input, index);
      $1.bytes[16 - (index + 1)] = FIX2INT(element);
    }
}

%typemap (out) pn_uuid_t
{
  int index;

  $result = rb_ary_new2(16);
  for(index = 0; index < 16; index++)
    {
      rb_ary_store($result, 16 - (index + 1), CHR2FIX($1.bytes[index]));
    }
}

int pn_message_load(pn_message_t *msg, char *STRING, size_t LENGTH);
%ignore pn_message_load;

int pn_message_load_data(pn_message_t *msg, char *STRING, size_t LENGTH);
%ignore pn_message_load_data;

int pn_message_load_text(pn_message_t *msg, char *STRING, size_t LENGTH);
%ignore pn_message_load_text;

int pn_message_load_amqp(pn_message_t *msg, char *STRING, size_t LENGTH);
%ignore pn_message_load_amqp;

int pn_message_load_json(pn_message_t *msg, char *STRING, size_t LENGTH);
%ignore pn_message_load_json;

int pn_message_encode(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_encode;

int pn_message_save(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save;

int pn_message_save_data(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_data;

int pn_message_save_text(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_text;

int pn_message_save_amqp(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_amqp;

int pn_message_save_json(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_json;

ssize_t pn_link_send(pn_link_t *transport, char *STRING, size_t LENGTH);
%ignore pn_link_send;

%rename(pn_link_recv) wrap_pn_link_recv;
%inline %{
  int wrap_pn_link_recv(pn_link_t *link, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_link_recv(link, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_link_recv;

ssize_t pn_transport_input(pn_transport_t *transport, char *STRING, size_t LENGTH);
%ignore pn_transport_input;

%rename(pn_transport_output) wrap_pn_transport_output;
%inline %{
  int wrap_pn_transport_output(pn_transport_t *transport, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_transport_output(transport, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_transport_output;

%rename(pn_delivery) wrap_pn_delivery;
%inline %{
  pn_delivery_t *wrap_pn_delivery(pn_link_t *link, char *STRING, size_t LENGTH) {
    return pn_delivery(link, pn_dtag(STRING, LENGTH));
  }
%}
%ignore pn_delivery;

// Suppress "Warning(451): Setting a const char * variable may leak memory." on pn_delivery_tag_t
%warnfilter(451) pn_delivery_tag_t;
%rename(pn_delivery_tag) wrap_pn_delivery_tag;
%inline %{
  void wrap_pn_delivery_tag(pn_delivery_t *delivery, char **ALLOC_OUTPUT, size_t *ALLOC_SIZE) {
    pn_delivery_tag_t tag = pn_delivery_tag(delivery);
    *ALLOC_OUTPUT = malloc(tag.size);
    *ALLOC_SIZE = tag.size;
    memcpy(*ALLOC_OUTPUT, tag.bytes, tag.size);
  }
%}
%ignore pn_delivery_tag;

%rename(pn_message_data) wrap_pn_message_data;
%inline %{
  int wrap_pn_message_data(char *STRING, size_t LENGTH, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_message_data(OUTPUT, *OUTPUT_SIZE, STRING, LENGTH);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_message_data;

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_cipher_name;

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_protocol_name;

%include "proton/cproton.i"
