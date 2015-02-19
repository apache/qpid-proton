%module cproton_perl

%{
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/sasl.h>
#include <proton/messenger.h>
#include <proton/ssl.h>
#include <proton/url.h>
#include <proton/reactor.h>
#include <proton/handlers.h>
%}

%include <cstring.i>

%typemap(in) pn_atom_t
{
  if(!$input)
    {
      $1.type = PN_NULL;
    }
  else
    {
      if(SvIOK($input)) // an integer value
        {
          $1.type = PN_LONG;
          $1.u.as_long = SvIV($input);
        }
      else if(SvNOK($input)) // a floating point value
        {
          $1.type = PN_FLOAT;
          $1.u.as_float = SvNV($input);
        }
      else if(SvPOK($input)) // a string type
        {
          STRLEN len;
          char* ptr;

          ptr = SvPV($input, len);
          $1.type = PN_STRING;
          $1.u.as_bytes.start = ptr;
          $1.u.as_bytes.size = strlen(ptr); // len;
        }
    }
}

%typemap(out) pn_atom_t
{
  SV* obj = sv_newmortal();

  switch($1.type)
    {
    case PN_NULL:
      sv_setsv(obj, &PL_sv_undef);
      break;

    case PN_BYTE:
      sv_setiv(obj, (IV)$1.u.as_byte);
      break;

    case PN_INT:
      sv_setiv(obj, (IV)$1.u.as_int);
      break;

    case PN_LONG:
      sv_setiv(obj, (IV)$1.u.as_long);
      break;

    case PN_STRING:
      {
        if($1.u.as_bytes.size > 0)
          {
            sv_setpvn(obj, $1.u.as_bytes.start, $1.u.as_bytes.size);
          }
        else
          {
            sv_setsv(obj, &PL_sv_undef);
          }
      }
      break;
    }

  $result = obj;
  // increment the hidden stack reference before returning
  argvi++;
}

%typemap(in) pn_bytes_t
{
  STRLEN len;
  char* ptr;

  ptr = SvPV($input, len);
  $1.start = ptr;
  $1.size = strlen(ptr);
}

%typemap(out) pn_bytes_t
{
  SV* obj = sv_newmortal();

  if($1.start != NULL)
    {
      $result = newSVpvn($1.start, $1.size);
    }
  else
    {
      $result = &PL_sv_undef;
    }

  argvi++;
}

%typemap(in) pn_decimal128_t
{
  AV *tmpav = (AV*)SvRV($input);
  int index = 0;

  for(index = 0; index < 16; index++)
    {
      $1.bytes[index] = SvIV(*av_fetch(tmpav, index, 0));
      $1.bytes[index] = $1.bytes[index] & 0xff;
    }
}

%typemap(out) pn_decimal128_t
{
  $result = newSVpvn($1.bytes, 16);
  argvi++;
}

%typemap(in) pn_uuid_t
{
  // XXX: I believe there is a typemap or something similar for
  // typechecking the input. We should probably use it.
  AV* tmpav = (AV *) SvRV($input);
  int index = 0;

  for(index = 0; index < 16; index++)
    {
      $1.bytes[index] = SvIV(*av_fetch(tmpav, index, 0));
      $1.bytes[index] = $1.bytes[index] & 0xff;
    }
}

%typemap(out) pn_uuid_t
{
  $result = newSVpvn($1.bytes, 16);
  argvi++;
}

%cstring_output_withsize(char *OUTPUT, size_t *OUTPUT_SIZE)
%cstring_output_allocate_size(char **ALLOC_OUTPUT, size_t *ALLOC_SIZE, free(*$1));

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
    *ALLOC_OUTPUT = (char *)malloc(tag.size);
    *ALLOC_SIZE = tag.size;
    memcpy(*ALLOC_OUTPUT, tag.start, tag.size);
  }
%}
%ignore pn_delivery_tag;

%include "proton/cproton.i"
