%module cproton

// provided by SWIG development libraries
%include php.swg


%header %{
/* Include the headers needed by the code in this wrapper file */
#include <proton/driver.h>
#include <wchar.h>

#define zend_error_noreturn zend_error
%}

%apply (char *STRING, int LENGTH) { (char *STRING, size_t LENGTH) };

// ssize_t return value
//
%typemap(out) ssize_t {
    ZVAL_LONG($result, (long)$1);
}


// wchar_t *WCHAR_INPUT
//
// support for wchar_t * input arguments: convert PHP string to wchar_t string.
//
%typemap(in) wchar_t * {
    if (Z_TYPE_PP($input) != IS_STRING) {
        convert_to_string_ex($input);
    }
    const char *src = Z_STRVAL_PP($input);
    const size_t inLen = Z_STRLEN_PP($input);

    if (src) {
      // determine size needed for converted buffer
      mbstate_t state = {};
      const char *tmp = src;
      size_t wlen = mbsnrtowcs( NULL, &tmp, inLen, 0, &state );
      if (wlen == (size_t)-1) {
        SWIG_PHP_Error(E_ERROR, "Cannot convert string argument $argnum of $symname to wchar_t string.");
      }
      // include additional nul terminator in case source does not include one
      $1 = malloc(sizeof(wchar_t) * (wlen + 2));

      tmp = src;
      wlen = mbsnrtowcs( $1, &tmp, inLen, wlen+1, &state);
      if (wlen == (size_t)-1) {
        SWIG_PHP_Error(E_ERROR, "Cannot convert string argument $argnum of $symname to wchar_t string.");
      }
      $1[wlen] = (wchar_t)0;
    } else {
      $1 == NULL;
    }
 }
%typemap(freearg) wchar_t * {
    free($1);   // free the buffer holding the wchar_t buffer
 }

// convert wchar_t * return value to a PHP string
%typemap(out) wchar_t * {
    // determine size needed for converted buffer
    const wchar_t *tmp = $1;
    if (tmp) {
      mbstate_t state = {};
      size_t slen = wcsrtombs( NULL, &tmp, 0, &state);
      if (slen == (size_t)-1) {
        SWIG_PHP_Error(E_ERROR, "Cannot convert wchar_t return value from $symname to string.");
      }

      char *str = emalloc(sizeof(char) * slen+1);
      tmp = $1;
      slen = wcsrtombs( str, &tmp, slen+1, &state);
      if (slen == (size_t)-1) {
        SWIG_PHP_Error(E_ERROR, "Cannot convert wchar_t return value from $symname to string.");
      }

      ZVAL_STRINGL($result, str, slen, 0);  // 0 == assume ownership of buffer
    } else {
      ZVAL_NULL($result);
    }
 }


// (char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN)
//
// typemap for binary buffer output arguments.  Given an uninitialized pointer for a
// buffer (OUTPUT_BUFFER) and a pointer to an un-initialized size/error (OUTPUT_LEN), a buffer
// will be allocated and filled with binary data. *OUTPUT_BUFFER will be set to the address
// of the allocated buffer.  *OUTPUT_LEN will be set to the size of the data.  The maximum
// length of the buffer must be provided by a separate argument.
//
// The return value is an array, with [0] set to the length of the output buffer OR an
// error code and [1] set to the returned string object.  This value is appended to the
// function's return value (also an array).
//
%typemap(in,numinputs=0) (char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) (char *Buff = 0, ssize_t outLen = 0) {
    // setup locals for output.
    $1 = &Buff;
    $2 = &outLen;
}
%typemap(argout,fragment="t_output_helper") (char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) {
    // convert to array: [0]=len||error, [1]=binary string
    zval *tmp;
    ALLOC_INIT_ZVAL(tmp);
    array_init(tmp);
    ssize_t len = *($2);
    add_next_index_long(tmp, len); // write the len|error code
    if (len >= 0) {
        add_next_index_stringl(tmp, *($1), len, 0);  // 0 == take ownership of $1 memory
    } else {
        add_next_index_string(tmp, "", 1);    // 1 = strdup the ""
    }
    t_output_helper(&$result, tmp);     // append it to output array
}


// The PHP SWIG typedefs define the typemap STRING, LENGTH to be binary safe (allow
// embedded \0's).
//

// allow pn_send/pn_input's input buffer to be binary safe
ssize_t pn_send(pn_link_t *transport, char *STRING, size_t LENGTH);
%ignore pn_send;
ssize_t pn_input(pn_transport_t *transport, char *STRING, size_t LENGTH);
%ignore pn_input;

ssize_t pn_sasl_send(pn_sasl_t *sasl, char *STRING, size_t LENGTH);
%ignore pn_sasl_send;
ssize_t pn_sasl_input(pn_sasl_t *sasl, char *STRING, size_t LENGTH);
%ignore pn_sasl_input;


// Use the OUTPUT_BUFFER,OUTPUT_LEN typemap to allow these functions to return
// variable length binary data.

%rename(pn_recv) wrap_pn_recv;
// in PHP:   array = pn_recv(link, MAXLEN);
//           array[0] = size || error code
//           array[1] = native string containing binary data
%inline %{
    void wrap_pn_recv(pn_link_t *link, size_t maxCount, char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) {
        *OUTPUT_BUFFER = emalloc(sizeof(char) * maxCount);
        *OUTPUT_LEN = pn_recv(link, *OUTPUT_BUFFER, maxCount );
    }
%}
%ignore pn_recv;

%rename(pn_sasl_recv) wrap_pn_sasl_recv;
// in PHP:   array = pn_sasl_recv(sasl, MAXLEN);
//           array[0] = size || error code
//           array[1] = native string containing binary data
%inline %{
    void wrap_pn_sasl_recv(pn_sasl_t *sasl, size_t maxCount, char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) {
        *OUTPUT_BUFFER = emalloc(sizeof(char) * maxCount);
        *OUTPUT_LEN = pn_sasl_recv( sasl, *OUTPUT_BUFFER, maxCount );
    }
%}
%ignore pn_sasl_recv;

%rename(pn_output) wrap_pn_output;
// in PHP:   array = pn_output(transport, MAXLEN);
//           array[0] = size || error code
//           array[1] = native string containing binary data
%inline %{
    void wrap_pn_output(pn_transport_t *transport, size_t maxCount, char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) {
        *OUTPUT_BUFFER = emalloc(sizeof(char) * maxCount);
        *OUTPUT_LEN = pn_output(transport, *OUTPUT_BUFFER, maxCount);
    }
%}
%ignore pn_output;

%rename(pn_sasl_output) wrap_pn_output;
// in PHP:   array = pn_sasl_output(sasl, MAXLEN);
//           array[0] = size || error code
//           array[1] = native string containing binary data
%inline %{
    void wrap_pn_sasl_output(pn_sasl_t *sasl, size_t maxCount, char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN) {
        *OUTPUT_BUFFER = emalloc(sizeof(char) * maxCount);
        *OUTPUT_LEN = pn_sasl_output(sasl, *OUTPUT_BUFFER, maxCount);
    }
%}
%ignore pn_sasl_output;

%rename(pn_message_data) wrap_pn_message_data;
// in PHP:  array = pn_message_data("binary message data", MAXLEN);
//          array[0] = size || error code
//          array[1] = native string containing binary data
%inline %{
    void wrap_pn_message_data(char *STRING, size_t LENGTH, char **OUTPUT_BUFFER, ssize_t *OUTPUT_LEN, size_t count) {
        *OUTPUT_BUFFER = emalloc(sizeof(char) * count);
        *OUTPUT_LEN = pn_message_data(*OUTPUT_BUFFER, count, STRING, LENGTH );
    }
%}
%ignore pn_message_data;



//
// allow pn_delivery/pn_delivery_tag to accept a binary safe string:
//

%rename(pn_delivery) wrap_pn_delivery;
// in PHP:   delivery = pn_delivery(link, "binary safe string");
//
%inline %{
  pn_delivery_t *wrap_pn_delivery(pn_link_t *link, char *STRING, size_t LENGTH) {
    return pn_delivery(link, pn_dtag(STRING, LENGTH));
  }
%}
%ignore pn_delivery;

// pn_delivery_tag: output a copy of the pn_delivery_tag buffer
//
%typemap(in,numinputs=0) (const char **RETURN_STRING, size_t *RETURN_LEN) (char *Buff = 0, size_t outLen = 0) {
    $1 = &Buff;         // setup locals for holding output values.
    $2 = &outLen;
}
%typemap(argout) (const char **RETURN_STRING, size_t *RETURN_LEN) {
    // This allocates a copy of the binary buffer for return to the caller
    ZVAL_STRINGL($result, *($1), *($2), 1); // 1 = duplicate the input buffer
}
%rename(pn_delivery_tag) wrap_pn_delivery_tag;
// in PHP: str = pn_delivery_tag(delivery);
//
%inline %{
    void wrap_pn_delivery_tag(pn_delivery_t *d, const char **RETURN_STRING, size_t *RETURN_LEN) {
        pn_delivery_tag_t tag = pn_delivery_tag(d);
        *RETURN_STRING = tag.bytes;
        *RETURN_LEN = tag.size;
    }
%}
%ignore pn_delivery_tag;



//
// reference counter management for passing a context to/from the listener/connector
//

%typemap(in) void *PHP_CONTEXT {
    // since we hold a pointer to the context we must increment the reference count
    Z_ADDREF_PP($input);
    $1 = *$input;
}

// return the context.  Apparently, PHP won't let us return a pointer to a reference
// counted zval, so we must return a copy of the data
%typemap(out) void * {
    *$result = *(zval *)($1);
    zval_copy_ctor($result);
}


// increment reference count of PHP_CONTEXT on input:
pn_listener_t *pn_listener(pn_driver_t *driver, const char *host, const char *port, void *PHP_CONTEXT);
%ignore pn_listener;

// increment reference count of PHP_CONTEXT on input:
pn_listener_t *pn_listener_fd(pn_driver_t *driver, int fd, void *PHP_CONTEXT);
%ignore pn_listener_fd;


%rename(pn_listener_context) wrap_pn_listener_context;
%inline {
    void *wrap_pn_listener_context(pn_listener_t *l) {
        zval *result = pn_listener_context(l);
        if (!result) {  // convert to PHP NULL
            ALLOC_INIT_ZVAL(result);
            ZVAL_NULL(result);
        }
        return result;
    }
}
%ignore pn_listener_context;

%rename(pn_listener_destroy) wrap_pn_listener_destroy;
%inline %{
  void wrap_pn_listener_destroy(pn_listener_t *l) {
      zval *obj = pn_listener_context(l);
      if (obj) {
          zval_ptr_dtor(&obj);  // drop the reference taken on input
      }
      pn_listener_destroy(l);
  }
%}
%ignore pn_listener_destroy;


// increment reference count of PHP_CONTEXT on input:
pn_connector_t *pn_connector(pn_driver_t *driver, const char *host, const char *port, void *PHP_CONTEXT);
%ignore pn_connector;

// increment reference count of PHP_CONTEXT on input:
pn_connector_t *pn_connector_fd(pn_driver_t *driver, int fd, void *PHP_CONTEXT);
%ignore pn_connector_fd;

%rename(pn_connector_context) wrap_pn_connector_context;
%inline {
    void *wrap_pn_connector_context(pn_connector_t *c)
    {
        zval *result = pn_connector_context(c);
        if (!result) {  // convert to PHP NULL
            ALLOC_INIT_ZVAL(result);
            ZVAL_NULL(result);
        }
        return result;
    }
}
%ignore pn_connector_context;

%rename(pn_connector_set_context) wrap_pn_connector_set_context;
%inline {
    void wrap_pn_connector_set_context(pn_connector_t *ctor, void *PHP_CONTEXT) {
        zval *old = pn_connector_context(ctor);
        if (old) {
            zval_ptr_dtor(&old);  // drop the reference taken on input
        }
        pn_connector_set_context(ctor, PHP_CONTEXT);
    }
}
%ignore pn_connector_set_context;

%rename(pn_connector_destroy) wrap_pn_connector_destroy;
%inline %{
  void wrap_pn_connector_destroy(pn_connector_t *c) {
      zval *obj = pn_connector_context(c);
      if (obj) {
          zval_ptr_dtor(&obj);  // drop the reference taken on input
      }
      pn_connector_destroy(c);
  }
%}
%ignore pn_connector_destroy;


%include "../cproton.i"
