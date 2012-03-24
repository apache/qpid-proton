%module cproton
%{
/* Includes the header in the wrapper code */
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/sasl.h>
#include <proton/driver.h>
%}

typedef unsigned int size_t;
typedef signed int ssize_t;

%include <cwstring.i>
%include <cstring.i>

%cstring_output_withsize(char *OUTPUT, size_t *OUTPUT_SIZE)
%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));

ssize_t pn_send(pn_link_t *transport, char *STRING, size_t LENGTH);
%ignore pn_send;

%rename(pn_recv) wrap_pn_recv;
%inline %{
  int wrap_pn_recv(pn_link_t *link, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_recv(link, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
      return 0;
    } else {
      *OUTPUT_SIZE = 0;
      return sz;
    }
  }
%}
%ignore pn_recv;

%rename(pn_output) wrap_pn_output;
%inline %{
  int wrap_pn_output(pn_transport_t *transport, char *OUTPUT, size_t *OUTPUT_SIZE) {
    ssize_t sz = pn_output(transport, OUTPUT, *OUTPUT_SIZE);
    if (sz >= 0) {
      *OUTPUT_SIZE = sz;
      return 0;
    } else {
      *OUTPUT_SIZE = 0;
      return sz;
    }
  }
%}
%ignore pn_output;

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
      return 0;
    } else {
      *OUTPUT_SIZE = 0;
      return sz;
    }
  }
%}
%ignore pn_message_data;

%rename(pn_acceptor) wrap_pn_acceptor;
%inline {
  pn_selectable_t *wrap_pn_acceptor(pn_driver_t *driver, const char *host, const char *port, PyObject *context) {
    Py_XINCREF(context);
    return pn_acceptor(driver, host, port, NULL, context);
  }
}
%ignore pn_acceptor;

%rename(pn_connector) wrap_pn_connector;
%inline {
  pn_selectable_t *wrap_pn_connector(pn_driver_t *driver, const char *host, const char *port, PyObject *context) {
    Py_XINCREF(context);
    return pn_connector(driver, host, port, NULL, context);
  }
}
%ignore pn_connector;

%rename(pn_selectable_context) wrap_pn_selectable_context;
%inline {
  PyObject *wrap_pn_selectable_context(pn_selectable_t *sel) {
    PyObject *result = pn_selectable_context(sel);
    Py_XINCREF(result);
    return result;
  }
}
%ignore pn_selectable_context;

%rename(pn_selectable_destroy) wrap_pn_selectable_destroy;
%inline %{
  void wrap_pn_selectable_destroy(pn_selectable_t *selectable) {
    PyObject *obj = pn_selectable_context(selectable);
    Py_XDECREF(obj);
    pn_selectable_destroy(selectable);
  }
%}
%ignore pn_selectable_destroy;

/* Parse the header file to generate wrappers */
%include "proton/engine.h"
%include "proton/message.h"
%include "proton/sasl.h"
%include "proton/driver.h"
