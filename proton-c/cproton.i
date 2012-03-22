%module cproton
%{
/* Includes the header in the wrapper code */
#include <proton/engine.h>
%}

typedef unsigned int size_t;
typedef signed int ssize_t;

%include <cwstring.i>
%include <cstring.i>

%cstring_output_withsize(char *OUTPUT, size_t *OUTPUT_SIZE)

int wrap_pn_output(pn_transport_t *transport, char *OUTPUT, size_t *OUTPUT_SIZE);

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

pn_delivery_t *wrap_pn_delivery(pn_link_t *link, char *STRING, size_t LENGTH);

%rename(pn_delivery) wrap_pn_delivery;

%inline %{
  pn_delivery_t *wrap_pn_delivery(pn_link_t *link, char *STRING, size_t LENGTH) {
    return pn_delivery(link, pn_dtag(STRING, LENGTH));
  }
%}

%ignore pn_delivery;

%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));

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

/* Parse the header file to generate wrappers */
%include "proton/engine.h"
