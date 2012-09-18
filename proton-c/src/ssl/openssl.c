/*
 *
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
 *
 */

#define _POSIX_C_SOURCE 1

#include <proton/ssl.h>
#include "./ssl-internal.h"
#include <proton/engine.h>
#include "../engine/engine-internal.h"
#include "../driver-internal.h"
#include "../util.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
//#include <sys/socket.h>


/** @file
 * SSL/TLS support API.
 *
 * This file contains an OpenSSL-based implemention of the SSL/TLS API.
 */

static int ssl_initialized;

typedef enum { SSL_MODE_CLIENT, SSL_MODE_SERVER } ssl_mode_t;
typedef enum { UNKNOWN_CONNECTION, SSL_CONNECTION, CLEAR_CONNECTION } connection_mode_t;

struct pn_ssl_t {
  SSL_CTX *ctx;
  SSL *ssl;
  ssl_mode_t mode;
  bool allow_unsecured;
  bool ca_db;           // true when CA database configured
  char *keyfile_pw;
  pn_ssl_verify_mode_t verify_mode;    // NEED INIT
  char *trusted_CAs;

  pn_transport_t *transport;

  BIO *bio_ssl;         // i/o from/to SSL socket layer
  BIO *bio_ssl_io;      // SSL "half" of network-facing BIO
  BIO *bio_net_io;      // socket-side "half" of network-facing BIO
  bool read_stalled;  // SSL has data to read, but client buffer is full.
  bool ssl_closed;      // SSL socket has closed
  bool ssl_shutdown;    // BIO_ssl_shutdown() called on socket.
  ssize_t app_closed;   // error code returned by upper layer


  // buffers for holding I/O from "applications" above SSL
#define APP_BUF_SIZE    PN_CONNECTOR_IO_BUF_SIZE
  char outbuf[APP_BUF_SIZE];
  size_t out_count;
  char inbuf[APP_BUF_SIZE];
  size_t in_count;

  // process cleartext i/o "above" the SSL layer
  ssize_t (*process_input)(pn_transport_t *, char *, size_t);
  ssize_t (*process_output)(pn_transport_t *, char *, size_t);

  pn_trace_t trace;
};


/* */
static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata);
static ssize_t process_input_ssl( pn_transport_t *transport, char *input_data, size_t len);
static ssize_t process_output_ssl( pn_transport_t *transport, char *input_data, size_t len);
static ssize_t process_input_cleartext(pn_transport_t *transport, char *input_data, size_t len);
static ssize_t process_output_cleartext(pn_transport_t *transport, char *buffer, size_t max_len);
static ssize_t process_input_unknown(pn_transport_t *transport, char *input_data, size_t len);
static ssize_t process_output_unknown(pn_transport_t *transport, char *input_data, size_t len);
static connection_mode_t check_for_ssl_connection( const char *data, size_t len );


// @todo: used to avoid littering the code with calls to printf...
static void _log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

// @todo: used to avoid littering the code with calls to printf...
static void _log(pn_ssl_t *ssl, const char *fmt, ...)
{
    if (PN_TRACE_DRV & ssl->trace) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}


// @todo replace with a "reasonable" default (?), allow application to register its own
// callback.
#if 0
static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    fprintf(stderr, "VERIFY_CALLBACK: pre-verify-ok=%d\n", preverify_ok);

           char    buf[256];
           X509   *err_cert;
           int     err, depth;

           err_cert = X509_STORE_CTX_get_current_cert(ctx);
           err = X509_STORE_CTX_get_error(ctx);
           depth = X509_STORE_CTX_get_error_depth(ctx);

           /*
            * Retrieve the pointer to the SSL of the connection currently treated
            * and the application specific data stored into the SSL object.
            */

           X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

           /*
            * Catch a too long certificate chain. The depth limit set using
            * SSL_CTX_set_verify_depth() is by purpose set to "limit+1" so
            * that whenever the "depth>verify_depth" condition is met, we
            * have violated the limit and want to log this error condition.
            * We must do it here, because the CHAIN_TOO_LONG error would not
            * be found explicitly; only errors introduced by cutting off the
            * additional certificates would be logged.
            */
           if (!preverify_ok) {
               printf("verify error:num=%d:%s:depth=%d:%s\n", err,
                        X509_verify_cert_error_string(err), depth, buf);
           }

           /*
            * At this point, err contains the last verification error. We can use
            * it for something special
            */
           if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
           {
             X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
             printf("issuer= %s\n", buf);
           }

    return 1;
}
#endif




/** Public API - visible to application code */


int pn_ssl_set_credentials( pn_ssl_t *ssl,
                            const char *certificate_file,
                            const char *private_key_file,
                            const char *password)
{
  if (!ssl) return 0;

  if (SSL_CTX_use_certificate_chain_file(ssl->ctx, certificate_file) != 1) {
    _log_error("SSL_CTX_use_certificate_chain_file( %s ) failed\n", certificate_file);
    return -3;
  }

  if (password) {
    ssl->keyfile_pw = pn_strdup(password);  // @todo: obfuscate me!!!
    SSL_CTX_set_default_passwd_cb(ssl->ctx, keyfile_pw_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ssl->ctx, ssl->keyfile_pw);
  }

  if (SSL_CTX_use_PrivateKey_file(ssl->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
    _log_error("SSL_CTX_use_PrivateKey_file( %s ) failed\n", private_key_file);
    return -4;
  }

  _log( ssl, "Configured local certificate file %s\n", certificate_file );
  return 0;
}


int pn_ssl_set_trusted_ca_db(pn_ssl_t *ssl,
                             const char *certificate_db)
{
  if (!ssl) return 0;

  // certificates can be either a file or a directory, which determines how it is passed
  // to SSL_CTX_load_verify_locations()
  struct stat sbuf;
  if (stat( certificate_db, &sbuf ) != 0) {
    _log_error("stat(%s) failed: %s\n", certificate_db, strerror(errno));
    return -1;
  }

  const char *file;
  const char *dir;
  if (S_ISDIR(sbuf.st_mode)) {
    dir = certificate_db;
    file = NULL;
  } else {
    dir = NULL;
    file = certificate_db;
  }

  if (SSL_CTX_load_verify_locations( ssl->ctx, file, dir ) != 1) {
    _log_error("SSL_CTX_load_verify_locations( %s ) failed\n", certificate_db);
    return -1;
  }

  _log( ssl, "loaded trusted CA database: file=%s dir=%s\n", file, dir );
  return 0;
}


int pn_ssl_allow_unsecured_client(pn_ssl_t *ssl)
{
  if (ssl) {
    if (ssl->mode != SSL_MODE_SERVER) {
      _log_error("Cannot permit unsecured clients - not a server.\n");
      return -1;
    }
    ssl->allow_unsecured = true;
    ssl->process_input = process_input_unknown;
    ssl->process_output = process_output_unknown;
    _log( ssl, "Allowing connections from unsecured clients.\n" );
  }
  return 0;
}


int pn_ssl_set_peer_authentication(pn_ssl_t *ssl,
                                   const pn_ssl_verify_mode_t mode,
                                   const char *trusted_CAs)
{
  if (!ssl) return 0;

  switch (mode) {
  case PN_SSL_VERIFY_PEER:

    if (ssl->mode == SSL_MODE_SERVER) {
      // openssl requires that server connections supply a list of trusted CAs which is
      // sent to the client
      if (!trusted_CAs) {
        _log_error("Error: a list of trusted CAs must be provided.\n");
        return -1;
      }

      ssl->trusted_CAs = pn_strdup( trusted_CAs );
      STACK_OF(X509_NAME) *cert_names;
      cert_names = SSL_load_client_CA_file( ssl->trusted_CAs );
      if (cert_names != NULL)
        SSL_set_client_CA_list(ssl->ssl, cert_names);
      else {
        _log_error("Unable to process file of trusted CAs: %s\n", trusted_CAs);
        return -1;
      }
    }

    SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    // verify_callback /*?verify callback?*/ );
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(ssl->ctx, 1);
#endif
    ssl->verify_mode = PN_SSL_VERIFY_PEER;
    break;

  case PN_SSL_NO_VERIFY_PEER:
    SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_NONE, NULL );
    ssl->verify_mode = PN_SSL_NO_VERIFY_PEER;
    break;

  default:
    _log_error( "Invalid peer authentication mode given.\n" );
    return -1;
  }

    _log( ssl, "Peer authentication mode set to %s\n", (ssl->verify_mode == PN_SSL_VERIFY_PEER) ? "VERIFY-PEER" : "NO-VERIFY-PEER");
    return 0;
}


int pn_ssl_get_peer_authentication(pn_ssl_t *ssl,
                                   pn_ssl_verify_mode_t *mode,
                                   char *trusted_CAs, size_t *trusted_CAs_size)
{
  if (!ssl) return -1;

  if (mode) *mode = ssl->verify_mode;
  if (trusted_CAs && trusted_CAs_size && *trusted_CAs_size) {
    if (ssl->trusted_CAs) {
      strncpy( trusted_CAs, ssl->trusted_CAs, *trusted_CAs_size );
      trusted_CAs[*trusted_CAs_size - 1] = '\0';
      *trusted_CAs_size = strlen(ssl->trusted_CAs) + 1;
    } else {
      *trusted_CAs = '\0';
      *trusted_CAs_size = 0;
    }
  } else if (trusted_CAs_size) {
    *trusted_CAs_size = (ssl->trusted_CAs) ? strlen(ssl->trusted_CAs) + 1 : 0;
  }
  return 0;
}


pn_ssl_t *pn_ssl_server(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) {
    if (transport->ssl->mode != SSL_MODE_SERVER) {
      _log_error("Error: transport already configured as a client.\n");
      return NULL;
    }
    return transport->ssl;
  }

  if (!ssl_initialized) {
    ssl_initialized = 1;
    SSL_library_init();
    SSL_load_error_strings();
  }

  pn_ssl_t *ssl = calloc(1, sizeof(pn_ssl_t));
  if (!ssl) return NULL;

  ssl->ctx = SSL_CTX_new(SSLv23_server_method());
  if (!ssl->ctx) {
    _log_error("Unable to initialize SSL context: %s\n", strerror(errno));
    free(ssl);
    return NULL;
  }
  ssl->verify_mode = PN_SSL_NO_VERIFY_PEER;
  SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_NONE, NULL );        // default: no client authentication

  ssl->ssl = SSL_new(ssl->ctx);
  if (!ssl->ssl) {
    _log_error( "SSL socket setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  SSL_set_accept_state(ssl->ssl);

  // now layer a BIO over the SSL socket
  ssl->bio_ssl = BIO_new(BIO_f_ssl());
  if (!ssl->bio_ssl) {
    _log_error( "BIO setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  (void)BIO_set_ssl(ssl->bio_ssl, ssl->ssl, BIO_NOCLOSE);

  // create the "lower" BIO "pipe", and attach it below the SSL layer
  if (!BIO_new_bio_pair(&ssl->bio_ssl_io, 0, &ssl->bio_net_io, 0)) {
    _log_error( "BIO setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  SSL_set_bio(ssl->ssl, ssl->bio_ssl_io, ssl->bio_ssl_io);

  ssl->mode = SSL_MODE_SERVER;
  ssl->transport = transport;
  ssl->process_input = process_input_ssl;
  ssl->process_output = process_output_ssl;

  ssl->trace = PN_TRACE_OFF;

  return ssl;
}

pn_ssl_t *pn_ssl_client(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) {
    if (transport->ssl->mode != SSL_MODE_CLIENT) {
      _log_error("Error: transport already configured as a server.\n");
      return NULL;
    }
    return transport->ssl;
  }

  if (!ssl_initialized) {
    ssl_initialized = 1;
    SSL_library_init();
    SSL_load_error_strings();
  }

  pn_ssl_t *ssl = calloc(1, sizeof(pn_ssl_t));
  if (!ssl) return NULL;

  ssl->ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ssl->ctx) {
    _log_error("Unable to initialize SSL context: %s\n", strerror(errno));
    free(ssl);
    return NULL;
  }
  ssl->verify_mode = PN_SSL_VERIFY_PEER;
  SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL );
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(ssl->ctx, 1);
#endif

  ssl->ssl = SSL_new(ssl->ctx);
  if (!ssl->ssl) {
    _log_error( "SSL socket setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  SSL_set_connect_state(ssl->ssl);

  // now layer a BIO over the SSL socket
  ssl->bio_ssl = BIO_new(BIO_f_ssl());
  if (!ssl->bio_ssl) {
    _log_error( "BIO setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  (void)BIO_set_ssl(ssl->bio_ssl, ssl->ssl, BIO_NOCLOSE);

  // create the "lower" BIO "pipe", and attach it below the SSL layer
  if (!BIO_new_bio_pair(&ssl->bio_ssl_io, 0, &ssl->bio_net_io, 0)) {
    _log_error( "BIO setup failure.\n" );
    pn_ssl_free(ssl);
    return NULL;
  }
  SSL_set_bio(ssl->ssl, ssl->bio_ssl_io, ssl->bio_ssl_io);

  ssl->mode = SSL_MODE_CLIENT;

  ssl->transport = transport;
  ssl->process_input = process_input_ssl;
  ssl->process_output = process_output_ssl;

  ssl->trace = PN_TRACE_OFF;

  return ssl;
}

void pn_ssl_free( pn_ssl_t *ssl)
{
  if (!ssl) return;
  if (ssl->bio_ssl) BIO_free(ssl->bio_ssl);
  if (ssl->bio_ssl_io) BIO_free(ssl->bio_ssl_io);
  if (ssl->bio_net_io) BIO_free(ssl->bio_net_io);
  if (ssl->ssl) SSL_free(ssl->ssl);
  if (ssl->ctx) SSL_CTX_free(ssl->ctx);

  if (ssl->keyfile_pw) free(ssl->keyfile_pw);
  if (ssl->trusted_CAs) free(ssl->trusted_CAs);

  free(ssl);
}

// move data received from the network into the SSL layer
ssize_t pn_ssl_input(pn_ssl_t *ssl, char *bytes, size_t available)
{
  return ssl->process_input( ssl->transport, bytes, available );
}

// pull output from the SSL layer and move into network output buffers
ssize_t pn_ssl_output(pn_ssl_t *ssl, char *buffer, size_t max_size)
{
  return ssl->process_output( ssl->transport, buffer, max_size );
}


/** Private: */

static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata)
{
    strncpy(buf, (char *)userdata, size);   // @todo: un-obfuscate me!!!
    buf[size - 1] = '\0';
    return(strlen(buf));
}




static int start_ssl_shutdown( pn_ssl_t *ssl )
{
  if (!ssl->ssl_shutdown) {
    ssl->ssl_shutdown = true;
    BIO_ssl_shutdown( ssl->bio_ssl );
  }
  return 0;
}



static int setup_ssl_connection( pn_ssl_t *ssl )
{
  _log( ssl, "SSL connection detected.\n");
  ssl->process_input = process_input_ssl;
  ssl->process_output = process_output_ssl;
  return 0;
}

//////// SSL Connections

// transfer data between the SSL socket and the upper layer.  Returns true if any work was
// done.  This routine modifies the app buffers (outbuf and inbuf), and possibly sets the
// ssl_closed or app_closed flags.
static bool do_socket_io( pn_ssl_t *ssl )
{
  pn_transport_t *transport = ssl->transport;
  size_t total = 0;
  size_t activity;

  do {
    activity = 0;

    // get outgoing data from app layer

    if (!ssl->app_closed) {
      while (ssl->out_count < APP_BUF_SIZE) {
        ssize_t app_bytes = transport->process_output(transport, &ssl->outbuf[ssl->out_count], ssl->out_count);
        if (app_bytes > 0) {
          ssl->out_count += app_bytes;
          activity += app_bytes;
        } else {
          if (app_bytes < 0) {
            _log(ssl, "Application layer closed: %d\n", (int) app_bytes);
            ssl->app_closed = app_bytes;
            if (app_bytes == PN_EOS) {
              if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
                pn_dispatcher_trace(transport->disp, 0, "-> EOS\n");
            } else {
              if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
                pn_dispatcher_trace(transport->disp, 0, "-> EOS (%zi) %s\n", app_bytes,
                                    pn_error_text(transport->error));
            }
          }
          break;
        }
      }
    }

    // write outgoing data to socket

    if (!ssl->ssl_closed) {
      char *data = ssl->outbuf;
      while (ssl->out_count > 0) {
        int written = BIO_write( ssl->bio_ssl, data, ssl->out_count );
        if (written > 0) {
          data += written;
          ssl->out_count -= written;
          activity += written;
        } else if (!BIO_should_retry(ssl->bio_ssl)) {
          _log(ssl, "Write to SSL socket failed - SSL connection closed!!\n");
          ssl->ssl_closed = true;
          break;
        }
      }
      if (!ssl->ssl_closed && ssl->out_count > 0 && data != ssl->outbuf)
        memmove( ssl->outbuf, data, ssl->out_count );
    }

    if (ssl->ssl_closed) {
      ssl->out_count = 0;       // cannot write to socket, so erase app output data
    }

    // read incoming data from socket

    if (!ssl->ssl_closed) {
      while ((APP_BUF_SIZE - ssl->in_count) > 0) {
        int written = BIO_read( ssl->bio_ssl, &ssl->inbuf[ssl->in_count], APP_BUF_SIZE - ssl->in_count );
        if (written > 0) {
          ssl->in_count += written;
          activity += written;
        } else if (!BIO_should_retry(ssl->bio_ssl)) {
          _log(ssl, "Read from SSL socket failed - SSL connection closed!!\n");
          ssl->ssl_closed = true;
          break;
        }
      }
    }

    // write incoming data to app layer

    if (!ssl->app_closed) {
      char *data = ssl->inbuf;
      while (ssl->in_count > 0) {
        ssize_t consumed = transport->process_input(transport, data, ssl->in_count);
        if (consumed > 0) {
          ssl->in_count -= consumed;
          data += consumed;
          activity += consumed;
        } else {
          if (consumed < 0) {
            _log(ssl, "Application layer closed: %d\n", (int) consumed);
            ssl->app_closed = consumed;
            if (consumed == PN_EOS) {
              if (transport->disp->trace & (PN_TRACE_RAW | PN_TRACE_FRM))
                pn_dispatcher_trace(transport->disp, 0, "<- EOS\n");
            } else {
              pn_dispatcher_trace(transport->disp, 0, "ERROR[%i] %s\n",
                                  pn_error_code(transport->error),
                                  pn_error_text(transport->error));
            }
          }
          break;
        }
      }
      if (!ssl->app_closed && ssl->in_count > 0 && data != ssl->inbuf)
        memmove( ssl->inbuf, data, ssl->in_count );
    }

    if (ssl->app_closed) {
      ssl->in_count = 0;        // cannot accept more input, drop it
    }

    total += activity;

  } while (activity);

  return total > 0 ? true : false;
}


static ssize_t process_input_ssl( pn_transport_t *transport, char *input_data, size_t available)
{
  pn_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_ERR;

  ssize_t consumed = 0;
  bool activity;

  do {

    activity = false;

    // Write to network bio as much as possible, consuming bytes/available
    while (available) {
      int written = BIO_write( ssl->bio_net_io, input_data, available );
      if (written < 1) break;
      input_data += written;
      available -= written;
      consumed += written;
      activity = true;
    }

    // process any work available at the SSL socket or application

    if (do_socket_io(ssl))
      activity = true;

  } while (activity);

  if (consumed == 0 && ssl->ssl_closed && ssl->app_closed)
    return ssl->app_closed;

  _log(ssl, "Processed %d bytes from transport input.\n", (int) consumed );
  return consumed;
}

static ssize_t process_output_ssl( pn_transport_t *transport, char *buffer, size_t max_len)
{
  pn_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_ERR;

  ssize_t written = 0;
  bool activity;

  do {
    activity = false;

    // process any work available at the SSL socket or application
    if (do_socket_io(ssl))
      activity = true;

    // read from the network bio as much as possible, filling the buffer
    while (max_len) {
      int available = BIO_read( ssl->bio_net_io, buffer, max_len );
      if (available < 1) break;
      max_len -= available;
      buffer += available;
      written += available;
      activity = true;
    }
  } while (activity);

  // if the app is closed, and we've written any remaining app output to the socket, then
  // start the SSL shutdown handshake
  if (!ssl->ssl_shutdown && ssl->app_closed && ssl->out_count == 0) {
    start_ssl_shutdown(ssl);
  }

  if (written == 0 && ssl->ssl_closed && ssl->app_closed)
    return ssl->app_closed;
  _log(ssl, "Created %d bytes for transport output.\n", (int) written );
  return written;
}


//////// CLEARTEXT CONNECTIONS

static ssize_t process_input_cleartext(pn_transport_t *transport, char *input_data, size_t len)
{
  // just write directly to layer "above" SSL
  return transport->process_input( transport, input_data, len );
}

static ssize_t process_output_cleartext(pn_transport_t *transport, char *buffer, size_t max_len)
{
  // just read directly from the layer "above" SSL
  return transport->process_input( transport, buffer, max_len );
}



static int setup_cleartext_connection( pn_ssl_t *ssl )
{
  _log( ssl, "Cleartext connection detected.\n");
  ssl->process_input = process_input_cleartext;
  ssl->process_output = process_output_cleartext;
  return 0;
}


// until we determine if the client is using SSL or not:

static ssize_t process_input_unknown(pn_transport_t *transport, char *input_data, size_t len)
{
  switch (check_for_ssl_connection( input_data, len )) {
  case SSL_CONNECTION:
    setup_ssl_connection( transport->ssl );
    return transport->ssl->process_input( transport, input_data, len );
  case CLEAR_CONNECTION:
    setup_cleartext_connection( transport->ssl );
    return transport->ssl->process_input( transport, input_data, len );
  default:
    return 0;
  }
}

static ssize_t process_output_unknown(pn_transport_t *transport, char *input_data, size_t len)
{
  // do not do output until we know if SSL is used or not
  return 0;
}

static connection_mode_t check_for_ssl_connection( const char *data, size_t len )
{
  if (len >= 5) {
    const unsigned char *buf = (unsigned char *)data;
    /*
     * SSLv2 Client Hello format
     * http://www.mozilla.org/projects/security/pki/nss/ssl/draft02.html
     *
     * Bytes 0-1: RECORD-LENGTH
     * Byte    2: MSG-CLIENT-HELLO (1)
     * Byte    3: CLIENT-VERSION-MSB
     * Byte    4: CLIENT-VERSION-LSB
     *
     * Allowed versions:
     * 2.0 - SSLv2
     * 3.0 - SSLv3
     * 3.1 - TLS 1.0
     * 3.2 - TLS 1.1
     * 3.3 - TLS 1.2
     *
     * The version sent in the Client-Hello is the latest version supported by
     * the client. NSS may send version 3.x in an SSLv2 header for
     * maximum compatibility.
     */
    int isSSL2Handshake = buf[2] == 1 &&   // MSG-CLIENT-HELLO
      ((buf[3] == 3 && buf[4] <= 3) ||    // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)
       (buf[3] == 2 && buf[4] == 0));     // SSL 2

    /*
     * SSLv3/TLS Client Hello format
     * RFC 2246
     *
     * Byte    0: ContentType (handshake - 22)
     * Bytes 1-2: ProtocolVersion {major, minor}
     *
     * Allowed versions:
     * 3.0 - SSLv3
     * 3.1 - TLS 1.0
     * 3.2 - TLS 1.1
     * 3.3 - TLS 1.2
     */
    int isSSL3Handshake = buf[0] == 22 &&  // handshake
      (buf[1] == 3 && buf[2] <= 3);       // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)

    if (isSSL2Handshake || isSSL3Handshake) {
      return SSL_CONNECTION;
    } else {
      return CLEAR_CONNECTION;
    }
  }
  return UNKNOWN_CONNECTION;
}

void pn_ssl_trace(pn_ssl_t *ssl, pn_trace_t trace)
{
  ssl->trace = trace;
}
