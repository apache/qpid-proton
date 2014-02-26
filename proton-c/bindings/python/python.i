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
/* Includes the header in the wrapper code */
#if defined(_WIN32) && ! defined(__CYGWIN__)
#include <winsock2.h>
#endif
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/sasl.h>
#include <proton/driver.h>
#include <proton/driver_extras.h>
#include <proton/messenger.h>
#include <proton/ssl.h>
%}

%include <cstring.i>

%cstring_output_withsize(char *OUTPUT, size_t *OUTPUT_SIZE)
%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));
%cstring_output_maxsize(char *OUTPUT, size_t MAX_OUTPUT_SIZE)

%typemap(in) pn_bytes_t {
  if ($input == Py_None) {
    $1.start = NULL;
    $1.size = 0;
  } else {
    $1.start = PyString_AsString($input);
    if (!$1.start) {
      return NULL;
    }
    $1.size = PyString_Size($input);
  }
}

%typemap(out) pn_bytes_t {
  $result = PyString_FromStringAndSize($1.start, $1.size);
}

%typemap(in) pn_decimal128_t {
  memmove($1.bytes, PyString_AsString($input), 16);
}

%typemap(out) pn_decimal128_t {
  $result = PyString_FromStringAndSize($1.bytes, 16);
}

%typemap(in) pn_uuid_t {
  memmove($1.bytes, PyString_AsString($input), 16);
}

%typemap(out) pn_uuid_t {
  $result = PyString_FromStringAndSize($1.bytes, 16);
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

int pn_transport_push(pn_transport_t *transport, char *STRING, size_t LENGTH);
%ignore pn_transport_push;

%rename(pn_transport_peek) wrap_pn_transport_peek;
%inline %{
  int wrap_pn_transport_peek(pn_transport_t *transport, char *OUTPUT, size_t *OUTPUT_SIZE) {
    return pn_transport_peek(transport, OUTPUT, *OUTPUT_SIZE);
  }
%}
%ignore pn_transport_peek;

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
    *ALLOC_OUTPUT = (char *) malloc(tag.size);
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

%rename(pn_listener_set_context) wrap_pn_listener_set_context;
%inline {
  void wrap_pn_listener_set_context(pn_listener_t *l, PyObject *context) {
    // don't incref context: we 'borrow' the reference - prevents
    // reference loops.  Should be safe as the Python object must
    // outlive the C object.
    pn_listener_set_context(l, context);
  }
}
%ignore pn_listener_set_context;

%rename(pn_listener_context) wrap_pn_listener_context;
%inline {
  PyObject *wrap_pn_listener_context(pn_listener_t *l) {
    PyObject *result = (PyObject *) pn_listener_context(l);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_listener_context;

%rename(pn_connector_set_context) wrap_pn_connector_set_context;
%inline {
  void wrap_pn_connector_set_context(pn_connector_t *c, PyObject *context) {
    // don't incref context: we 'borrow' the reference - prevents
    // reference loops.  Should be safe as the Python object must
    // outlive the C object.
    pn_connector_set_context(c, context);
  }
}
%ignore pn_connector_set_context;

%rename(pn_connector_context) wrap_pn_connector_context;
%inline {
  PyObject *wrap_pn_connector_context(pn_connector_t *c) {
    PyObject *result = (PyObject *) pn_connector_context(c);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_connector_context;

%rename(pn_connection_get_context) wrap_pn_connection_get_context;
%inline {
  PyObject *wrap_pn_connection_get_context(pn_connection_t *c) {
    PyObject *result = (PyObject *) pn_connection_get_context(c);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_connection_get_context;

%rename(pn_connection_set_context) wrap_pn_connection_set_context;
%inline {
  void wrap_pn_connection_set_context(pn_connection_t *c, PyObject *context) {
    // don't incref context: we 'borrow' the reference
    pn_connection_set_context(c, context);
  }
}
%ignore pn_connection_set_context;

%rename(pn_session_get_context) wrap_pn_session_get_context;
%inline {
  PyObject *wrap_pn_session_get_context(pn_session_t *s) {
    PyObject *result = (PyObject *) pn_session_get_context(s);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_session_get_context;

%rename(pn_session_set_context) wrap_pn_session_set_context;
%inline {
  void wrap_pn_session_set_context(pn_session_t *s, PyObject *context) {
    // don't incref context: we 'borrow' the reference
    pn_session_set_context(s, context);
  }
}
%ignore pn_session_set_context;

%rename(pn_link_get_context) wrap_pn_link_get_context;
%inline {
  PyObject *wrap_pn_link_get_context(pn_link_t *l) {
    PyObject *result = (PyObject *) pn_link_get_context(l);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_link_get_context;

%rename(pn_link_set_context) wrap_pn_link_set_context;
%inline {
  void wrap_pn_link_set_context(pn_link_t *l, PyObject *context) {
    // don't incref context: we 'borrow' the reference
    pn_link_set_context(l, context);
  }
}
%ignore pn_link_set_context;

%rename(pn_delivery_get_context) wrap_pn_delivery_get_context;
%inline {
  PyObject *wrap_pn_delivery_get_context(pn_delivery_t *d) {
    PyObject *result = (PyObject *) pn_delivery_get_context(d);
    // incref the returned context, as the caller expects this
    if (result) {
      Py_INCREF(result);
      return result;
    } else {
      Py_RETURN_NONE;
    }
  }
}
%ignore pn_delivery_get_context;

%rename(pn_delivery_set_context) wrap_pn_delivery_set_context;
%inline {
  void wrap_pn_delivery_set_context(pn_delivery_t *d, PyObject *context) {
    // don't incref context: we 'borrow' the reference
    pn_delivery_set_context(d, context);
  }
}
%ignore pn_delivery_set_context;

ssize_t pn_data_decode(pn_data_t *data, char *STRING, size_t LENGTH);
%ignore pn_data_decode;

%rename(pn_data_encode) wrap_pn_data_encode;
%inline %{
  int wrap_pn_data_encode(pn_data_t *data, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_data_encode(data, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_data_encode;

%rename(pn_sasl_recv) wrap_pn_sasl_recv;
%inline %{
  int wrap_pn_sasl_recv(pn_sasl_t *sasl, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_sasl_recv(sasl, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
    } else {
      *OUTPUT_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_sasl_recv;

int pn_data_format(pn_data_t *data, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_data_format;

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_cipher_name;

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_protocol_name;

int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_ssl_get_peer_hostname;


%include "proton/cproton.i"
