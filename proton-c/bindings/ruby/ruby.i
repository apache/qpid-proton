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
#include <proton/messenger.h>
#include <proton/ssl.h>
#include <proton/types.h>
#include <proton/url.h>
#include <proton/reactor.h>
#include <proton/handlers.h>

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
  $1 = NUM2ULL($input);
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

int pn_message_encode(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_encode;

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

%rename(pn_transport_peek) wrap_pn_transport_peek;
%inline %{
  int wrap_pn_transport_peek(pn_transport_t *transport, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_transport_peek(transport, OUTPUT, *OUTPUT_SIZE);
    if(sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_transport_peek;

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
    memcpy(*ALLOC_OUTPUT, tag.start, tag.size);
  }
%}
%ignore pn_delivery_tag;

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_cipher_name;

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_protocol_name;

%inline %{
#if defined(RUBY20) || defined(RUBY21)

  typedef void *non_blocking_return_t;
#define RB_BLOCKING_CALL rb_thread_call_without_gvl

#elif defined(RUBY19)

    typedef VALUE non_blocking_return_t;
#define RB_BLOCKING_CALL rb_thread_blocking_region

#endif
  %}

%rename(pn_messenger_send) wrap_pn_messenger_send;
%rename(pn_messenger_recv) wrap_pn_messenger_recv;
%rename(pn_messenger_work) wrap_pn_messenger_work;

%inline %{

#if defined(RB_BLOCKING_CALL)

    static non_blocking_return_t pn_messenger_send_no_gvl(void *args) {
    VALUE result = Qnil;
    pn_messenger_t *messenger = (pn_messenger_t *)((void **)args)[0];
    int *limit = (int *)((void **)args)[1];

    int rc = pn_messenger_send(messenger, *limit);

    result = INT2NUM(rc);
    return (non_blocking_return_t )result;
    }

    static non_blocking_return_t pn_messenger_recv_no_gvl(void *args) {
    VALUE result = Qnil;
    pn_messenger_t *messenger = (pn_messenger_t *)((void **)args)[0];
    int *limit = (int *)((void **)args)[1];

    int rc = pn_messenger_recv(messenger, *limit);

    result = INT2NUM(rc);
    return (non_blocking_return_t )result;
  }

    static non_blocking_return_t pn_messenger_work_no_gvl(void *args) {
      VALUE result = Qnil;
      pn_messenger_t *messenger = (pn_messenger_t *)((void **)args)[0];
      int *timeout = (int *)((void **)args)[1];

      int rc = pn_messenger_work(messenger, *timeout);

      result = INT2NUM(rc);
      return (non_blocking_return_t )result;
    }

#endif

  int wrap_pn_messenger_send(pn_messenger_t *messenger, int limit) {
    int result = 0;

#if defined(RB_BLOCKING_CALL)

    // only release the gil if we're blocking
    if(pn_messenger_is_blocking(messenger)) {
      VALUE rc;
      void* args[2];

      args[0] = messenger;
      args[1] = &limit;

      rc = RB_BLOCKING_CALL(pn_messenger_send_no_gvl,
                            &args, RUBY_UBF_PROCESS, NULL);

      if(RTEST(rc))
        {
          result = FIX2INT(rc);
        }
    }

#else // !defined(RB_BLOCKING_CALL)
    result = pn_messenger_send(messenger, limit);
#endif // defined(RB_BLOCKING_CALL)

    return result;
  }

  int wrap_pn_messenger_recv(pn_messenger_t *messenger, int limit) {
    int result = 0;

#if defined(RB_BLOCKING_CALL)
    // only release the gil if we're blocking
    if(pn_messenger_is_blocking(messenger)) {
      VALUE rc;
      void* args[2];

      args[0] = messenger;
      args[1] = &limit;

      rc = RB_BLOCKING_CALL(pn_messenger_recv_no_gvl,
                            &args, RUBY_UBF_PROCESS, NULL);

      if(RTEST(rc))
        {
          result = FIX2INT(rc);
        }

    } else {
      result = pn_messenger_recv(messenger, limit);
    }
#else // !defined(RB_BLOCKING_CALL)
    result = pn_messenger_recv(messenger, limit);
#endif // defined(RB_BLOCKING_CALL)

      return result;
  }

  int wrap_pn_messenger_work(pn_messenger_t *messenger, int timeout) {
    int result = 0;

#if defined(RB_BLOCKING_CALL)
    // only release the gil if we're blocking
    if(timeout) {
      VALUE rc;
      void* args[2];

      args[0] = messenger;
      args[1] = &timeout;

      rc = RB_BLOCKING_CALL(pn_messenger_work_no_gvl,
                            &args, RUBY_UBF_PROCESS, NULL);

      if(RTEST(rc))
        {
          result = FIX2INT(rc);
        }
    } else {
      result = pn_messenger_work(messenger, timeout);
    }
#else
    result = pn_messenger_work(messenger, timeout);
#endif

    return result;
  }

%}

%ignore pn_messenger_send;
%ignore pn_messenger_recv;
%ignore pn_messenger_work;

%inline %{

#define CID_Pn_rbkey CID_pn_void

typedef struct {
  void *registry;
  char *method;
  char *key_value;
} Pn_rbkey_t;

void Pn_rbkey_initialize(Pn_rbkey_t *rbkey) {
  assert(rbkey);
  rbkey->registry = NULL;
  rbkey->method = NULL;
  rbkey->key_value = NULL;
}

void Pn_rbkey_finalize(Pn_rbkey_t *rbkey) {
  if(rbkey && rbkey->registry && rbkey->method && rbkey->key_value) {
    rb_funcall((VALUE )rbkey->registry, rb_intern(rbkey->method), 1, rb_str_new2(rbkey->key_value));
  }
  if(rbkey->key_value) {
    free(rbkey->key_value);
    rbkey->key_value = NULL;
  }
}

#define Pn_rbkey_inspect NULL
#define Pn_rbkey_compare NULL
#define Pn_rbkey_hashcode NULL

PN_CLASSDEF(Pn_rbkey)

void Pn_rbkey_set_registry(Pn_rbkey_t *rbkey, void *registry) {
  assert(rbkey);
  rbkey->registry = registry;
}

void *Pn_rbkey_get_registry(Pn_rbkey_t *rbkey) {
  assert(rbkey);
  return rbkey->registry;
}

void Pn_rbkey_set_method(Pn_rbkey_t *rbkey, char *method) {
  assert(rbkey);
  rbkey->method = method;
}

char *Pn_rbkey_get_method(Pn_rbkey_t *rbkey) {
  assert(rbkey);
  return rbkey->method;
}

void Pn_rbkey_set_key_value(Pn_rbkey_t *rbkey, char *key_value) {
  assert(rbkey);
  rbkey->key_value = malloc(strlen(key_value) + 1);
  strncpy(rbkey->key_value, key_value, strlen(key_value) + 1);
}

char *Pn_rbkey_get_key_value(Pn_rbkey_t *rbkey) {
  assert(rbkey);
  return rbkey->key_value;
}

Pn_rbkey_t *pni_void2rbkey(void *object) {
  return (Pn_rbkey_t *)object;
}

VALUE pn_void2rb(void *object) {
  return (VALUE )object;
}

void *pn_rb2void(VALUE object) {
  return (void *)object;
}

VALUE pni_address_of(void *object) {
  return ULL2NUM((unsigned long )object);
}

%}

//%rename(pn_collector_put) wrap_pn_collector_put;
//%inline %{
//  pn_event_t *wrap_pn_collector_put(pn_collector_t *collector, void *context,
//                               pn_event_type_t type) {
//    return pn_collector_put(collector, PN_RBREF, context, type);
//  }
//  %}
//%ignore pn_collector_put;

int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_ssl_get_peer_hostname;

%inline %{

  VALUE pni_ruby_get_proton_module() {
    VALUE mQpid = rb_define_module("Qpid");
    return rb_define_module_under(mQpid, "Proton");
  }

  void pni_ruby_add_to_registry(VALUE key, VALUE value) {
    VALUE result = rb_funcall(pni_ruby_get_proton_module(), rb_intern("add_to_registry"), 2, key, value);
  }

  VALUE pni_ruby_get_from_registry(VALUE key) {
    rb_funcall(pni_ruby_get_proton_module(), rb_intern("get_from_registry"), 1, key);
  }

  void pni_ruby_delete_from_registry(VALUE stored_key) {
    rb_funcall(pni_ruby_get_proton_module(), rb_intern("delete_from_registry"), 1, stored_key);
  }

  typedef struct {
    VALUE handler_key;
  } Pni_rbhandler_t;

  static Pni_rbhandler_t *pni_rbhandler(pn_handler_t *handler) {
    return (Pni_rbhandler_t *) pn_handler_mem(handler);
  }

  static void pni_rbdispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
    Pni_rbhandler_t *rbh = pni_rbhandler(handler);
    VALUE rbhandler = pni_ruby_get_from_registry(rbh->handler_key);

    rb_funcall(rbhandler, rb_intern("dispatch"), 2, SWIG_NewPointerObj(event, SWIGTYPE_p_pn_event_t, 0), INT2FIX(type));
  }

  static void pni_rbhandler_finalize(pn_handler_t *handler) {
    Pni_rbhandler_t *rbh = pni_rbhandler(handler);
    pni_ruby_delete_from_registry(rbh->handler_key);
  }

  pn_handler_t *pn_rbhandler(VALUE handler) {
    pn_handler_t *chandler = pn_handler_new(pni_rbdispatch, sizeof(Pni_rbhandler_t), pni_rbhandler_finalize);
    Pni_rbhandler_t *rhy = pni_rbhandler(chandler);

    VALUE ruby_key = rb_class_new_instance(0, NULL, rb_cObject);
    pni_ruby_add_to_registry(ruby_key, handler);

    rhy->handler_key = ruby_key;

    return chandler;
  }

%}

%include "proton/cproton.i"
