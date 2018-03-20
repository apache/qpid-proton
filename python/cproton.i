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
#include <proton/url.h>
#include <proton/message.h>
#include <proton/object.h>
#include <proton/sasl.h>
#include <proton/messenger.h>
#include <proton/ssl.h>
#include <proton/reactor.h>
#include <proton/handlers.h>

/*
NOTE: According to ccache-swig man page: "Known problems are using
preprocessor directives within %inline blocks and the use of ’#pragma SWIG’."
This includes using macros in an %inline section.

Keep preprocessor directives and macro expansions in the normal header section.
*/

PN_HANDLE(PNI_PYTRACER);
%}

%include <cstring.i>

%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));
%cstring_output_maxsize(char *OUTPUT, size_t MAX_OUTPUT_SIZE)

%include <pybuffer.i>
%pybuffer_binary(const char *BIN_IN, size_t BIN_LEN)

// Typemap for methods that return binary data:
// force the return type as binary - this is necessary for Python3
%typemap(in,noblock=1,fragment=SWIG_AsVal_frag(size_t)) (char *BIN_OUT, size_t *BIN_SIZE)
(int res, size_t n, char *buff = 0, $*2_ltype size) {
  res = SWIG_AsVal(size_t)($input, &n);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "(char *BIN_OUT, size_t *BIN_SIZE)", $symname, $argnum);
  }
  buff= %new_array(n+1, char);
  $1 = %static_cast(buff, $1_ltype);
  size = %numeric_cast(n,$*2_ltype);
  $2 = &size;
}
%typemap(freearg,noblock=1,match="in")(char *BIN_OUT, size_t *BIN_SIZE) {
  if (buff$argnum) %delete_array(buff$argnum);
}
%typemap(argout,noblock=1) (char *BIN_OUT, size_t *BIN_SIZE) {
  %append_output(PyBytes_FromStringAndSize($1,*$2));
}

// Typemap for those methods that return variable length text data in a buffer
// provided as a parameter.  If the method fails we must avoid attempting to
// decode the contents of the buffer as it does not carry valid text data.
%typemap(in,noblock=1,fragment=SWIG_AsVal_frag(size_t)) (char *VTEXT_OUT, size_t *VTEXT_SIZE)
(int res, size_t n, char *buff = 0, $*2_ltype size) {
  res = SWIG_AsVal(size_t)($input, &n);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "(char *VTEXT_OUT, size_t *VTEXT_SIZE)", $symname, $argnum);
  }
  buff = %new_array(n+1, char);
  $1 = %static_cast(buff, $1_ltype);
  size = %numeric_cast(n,$*2_ltype);
  $2 = &size;
}
%typemap(freearg,noblock=1,match="in")(char *VTEXT_OUT, size_t *VTEXT_SIZE) {
  if (buff$argnum) %delete_array(buff$argnum);
}
%typemap(argout,noblock=1,fragment="SWIG_FromCharPtrAndSize") (char *VTEXT_OUT, size_t *VTEXT_SIZE) {
  %append_output(SWIG_FromCharPtrAndSize($1,*$2));
}


// These are not used/needed in the python binding
%ignore pn_dtag;
%ignore pn_message_get_id;
%ignore pn_message_set_id;
%ignore pn_message_get_correlation_id;
%ignore pn_message_set_correlation_id;

%typemap(in) pn_bytes_t {
  if ($input == Py_None) {
    $1.start = NULL;
    $1.size = 0;
  } else {
    $1.start = PyBytes_AsString($input);

    if (!$1.start) {
      return NULL;
    }
    $1.size = PyBytes_Size($input);
  }
}

%typemap(out) pn_bytes_t {
  $result = PyBytes_FromStringAndSize($1.start, $1.size);
}

%typemap(out) pn_delivery_tag_t {
  $result = PyBytes_FromStringAndSize($1.bytes, $1.size);
}

%typemap(in) pn_uuid_t {
  memset($1.bytes, 0, 16);
  if ($input == Py_None) {
    ; // Already zeroed out
  } else {
    const char* b = PyBytes_AsString($input);
    if (b) {
        memmove($1.bytes, b, (PyBytes_Size($input) < 16 ? PyBytes_Size($input) : 16));
    } else {
        return NULL;
    }
  }
}

%typemap(out) pn_uuid_t {
  $result = PyBytes_FromStringAndSize($1.bytes, 16);
}

%apply pn_uuid_t { pn_decimal128_t };

int pn_message_encode(pn_message_t *msg, char *BIN_OUT, size_t *BIN_SIZE);
%ignore pn_message_encode;

int pn_message_decode(pn_message_t *msg, const char *BIN_IN, size_t BIN_LEN);
%ignore pn_message_decode;

ssize_t pn_link_send(pn_link_t *transport, const char *BIN_IN, size_t BIN_LEN);
%ignore pn_link_send;

%rename(pn_link_recv) wrap_pn_link_recv;
%inline %{
  int wrap_pn_link_recv(pn_link_t *link, char *BIN_OUT, size_t *BIN_SIZE) {
    ssize_t sz = pn_link_recv(link, BIN_OUT, *BIN_SIZE);
    if (sz >= 0) {
      *BIN_SIZE = sz;
    } else {
      *BIN_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_link_recv;

ssize_t pn_transport_push(pn_transport_t *transport, const char *BIN_IN, size_t BIN_LEN);
%ignore pn_transport_push;

%rename(pn_transport_peek) wrap_pn_transport_peek;
%inline %{
  int wrap_pn_transport_peek(pn_transport_t *transport, char *BIN_OUT, size_t *BIN_SIZE) {
    ssize_t sz = pn_transport_peek(transport, BIN_OUT, *BIN_SIZE);
    if (sz >= 0) {
      *BIN_SIZE = sz;
    } else {
      *BIN_SIZE = 0;
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

%rename(pn_delivery_tag) wrap_pn_delivery_tag;
%inline %{
  void wrap_pn_delivery_tag(pn_delivery_t *delivery, char **ALLOC_OUTPUT, size_t *ALLOC_SIZE) {
    pn_delivery_tag_t tag = pn_delivery_tag(delivery);
    *ALLOC_OUTPUT = (char *) malloc(tag.size);
    *ALLOC_SIZE = tag.size;
    memcpy(*ALLOC_OUTPUT, tag.start, tag.size);
  }
%}
%ignore pn_delivery_tag;

ssize_t pn_data_decode(pn_data_t *data, const char *BIN_IN, size_t BIN_LEN);
%ignore pn_data_decode;

%rename(pn_data_encode) wrap_pn_data_encode;
%inline %{
  int wrap_pn_data_encode(pn_data_t *data, char *BIN_OUT, size_t *BIN_SIZE) {
    ssize_t sz = pn_data_encode(data, BIN_OUT, *BIN_SIZE);
    if (sz >= 0) {
      *BIN_SIZE = sz;
    } else {
      *BIN_SIZE = 0;
    }
    return sz;
  }
%}
%ignore pn_data_encode;

%rename(pn_data_format) wrap_pn_data_format;
%inline %{
  int wrap_pn_data_format(pn_data_t *data, char *VTEXT_OUT, size_t *VTEXT_SIZE) {
    int err = pn_data_format(data, VTEXT_OUT, VTEXT_SIZE);
    if (err) *VTEXT_SIZE = 0;
    return err;
  }
%}
%ignore pn_data_format;

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_cipher_name;

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE);
%ignore pn_ssl_get_protocol_name;

char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl, pn_ssl_cert_subject_subfield field);
%ignore pn_ssl_get_remote_subject_subfield;

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl, char *OUTPUT, size_t MAX_OUTPUT_SIZE, pn_ssl_hash_alg hash_alg);
%ignore pn_ssl_get_cert_fingerprint;

%rename(pn_ssl_get_peer_hostname) wrap_pn_ssl_get_peer_hostname;
%inline %{
  int wrap_pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *VTEXT_OUT, size_t *VTEXT_SIZE) {
    int err = pn_ssl_get_peer_hostname(ssl, VTEXT_OUT, VTEXT_SIZE);
    if (err) *VTEXT_SIZE = 0;
    return err;
  }
%}
%ignore pn_ssl_get_peer_hostname;


%immutable PN_PYREF;
%inline %{
  extern const pn_class_t *PN_PYREF;

  #define CID_pn_pyref CID_pn_void
  #define pn_pyref_new NULL
  #define pn_pyref_initialize NULL
  #define pn_pyref_finalize NULL
  #define pn_pyref_free NULL
  #define pn_pyref_hashcode pn_void_hashcode
  #define pn_pyref_compare pn_void_compare
  #define pn_pyref_inspect pn_void_inspect

  static void pn_pyref_incref(void *object) {
    PyObject* p = (PyObject*) object;
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    Py_XINCREF(p);
    SWIG_PYTHON_THREAD_END_BLOCK;
  }

  static void pn_pyref_decref(void *object) {
    PyObject* p = (PyObject*) object;
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    Py_XDECREF(p);
    SWIG_PYTHON_THREAD_END_BLOCK;
  }

  static int pn_pyref_refcount(void *object) {
    return 1;
  }

  static const pn_class_t *pn_pyref_reify(void *object) {
    return PN_PYREF;
  }

  const pn_class_t PNI_PYREF = PN_METACLASS(pn_pyref);
  const pn_class_t *PN_PYREF = &PNI_PYREF;

  void *pn_py2void(PyObject *object) {
    return object;
  }

  PyObject *pn_void2py(void *object) {
    if (object) {
      PyObject* p = (PyObject*) object;
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      Py_INCREF(p);
      SWIG_PYTHON_THREAD_END_BLOCK;
      return p;
    } else {
      Py_RETURN_NONE;
    }
  }

  PyObject *pn_cast_pn_void(void *object) {
    return pn_void2py(object);
  }

  typedef struct {
    PyObject *handler;
    PyObject *dispatch;
    PyObject *exception;
  } pni_pyh_t;

  static pni_pyh_t *pni_pyh(pn_handler_t *handler) {
    return (pni_pyh_t *) pn_handler_mem(handler);
  }

  static void pni_pyh_finalize(pn_handler_t *handler) {
    pni_pyh_t *pyh = pni_pyh(handler);
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    Py_DECREF(pyh->handler);
    Py_DECREF(pyh->dispatch);
    Py_DECREF(pyh->exception);
    SWIG_PYTHON_THREAD_END_BLOCK;
  }

  static void pni_pydispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
    pni_pyh_t *pyh = pni_pyh(handler);
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    PyObject *arg = SWIG_NewPointerObj(event, SWIGTYPE_p_pn_event_t, 0);
    PyObject *pytype = PyInt_FromLong(type);
    PyObject *result = PyObject_CallMethodObjArgs(pyh->handler, pyh->dispatch, arg, pytype, NULL);
    if (!result) {
      PyObject *exc, *val, *tb;
      PyErr_Fetch(&exc, &val, &tb);
      PyErr_NormalizeException(&exc, &val, &tb);
      if (!val) {
        val = Py_None;
        Py_INCREF(val);
      }
      if (!tb) {
        tb = Py_None;
        Py_INCREF(tb);
      }
      {
        PyObject *result2 = PyObject_CallMethodObjArgs(pyh->handler, pyh->exception, exc, val, tb, NULL);
        if (!result2) {
          PyErr_PrintEx(true);
        }
        Py_XDECREF(result2);
      }
      Py_XDECREF(exc);
      Py_XDECREF(val);
      Py_XDECREF(tb);
    }
    Py_XDECREF(arg);
    Py_XDECREF(pytype);
    Py_XDECREF(result);
    SWIG_PYTHON_THREAD_END_BLOCK;
  }

  pn_handler_t *pn_pyhandler(PyObject *handler) {
    pn_handler_t *chandler = pn_handler_new(pni_pydispatch, sizeof(pni_pyh_t), pni_pyh_finalize);
    pni_pyh_t *phy = pni_pyh(chandler);
    phy->handler = handler;
    {
      SWIG_PYTHON_THREAD_BEGIN_BLOCK;
      phy->dispatch = PyString_FromString("dispatch");
      phy->exception = PyString_FromString("exception");
      Py_INCREF(phy->handler);
      SWIG_PYTHON_THREAD_END_BLOCK;
    }
    return chandler;
  }

  void pn_pytracer(pn_transport_t *transport, const char *message) {
    PyObject *pytracer = (PyObject *) pn_record_get(pn_transport_attachments(transport), PNI_PYTRACER);
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    PyObject *pytrans = SWIG_NewPointerObj(transport, SWIGTYPE_p_pn_transport_t, 0);
    PyObject *pymsg = PyString_FromString(message);
    PyObject *result = PyObject_CallFunctionObjArgs(pytracer, pytrans, pymsg, NULL);
    if (!result) {
      PyErr_PrintEx(true);
    }
    Py_XDECREF(pytrans);
    Py_XDECREF(pymsg);
    Py_XDECREF(result);
    SWIG_PYTHON_THREAD_END_BLOCK;
  }

  void pn_transport_set_pytracer(pn_transport_t *transport, PyObject *obj) {
    pn_record_t *record = pn_transport_attachments(transport);
    pn_record_def(record, PNI_PYTRACER, PN_PYREF);
    pn_record_set(record, PNI_PYTRACER, obj);
    pn_transport_set_tracer(transport, pn_pytracer);
  }

  PyObject *pn_transport_get_pytracer(pn_transport_t *transport) {
    pn_record_t *record = pn_transport_attachments(transport);
    PyObject *obj = (PyObject *)pn_record_get(record, PNI_PYTRACER);
    if (obj) {
      Py_XINCREF(obj);
      return obj;
    } else {
      Py_RETURN_NONE;
    }
  }

%}

%include "proton/cproton.i"
