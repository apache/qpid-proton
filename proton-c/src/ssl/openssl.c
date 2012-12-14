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

#include <proton/ssl.h>
#include "./ssl-internal.h"
#include <proton/engine.h>
#include "../engine/engine-internal.h"
#include "../util.h"

#include <openssl/ssl.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>


/** @file
 * SSL/TLS support API.
 *
 * This file contains an OpenSSL-based implemention of the SSL/TLS API.
 */

static int ssl_initialized;
static int ssl_ex_data_index;

typedef enum { UNKNOWN_CONNECTION, SSL_CONNECTION, CLEAR_CONNECTION } connection_mode_t;

struct pn_ssl_t {
  SSL_CTX *ctx;
  SSL *ssl;
  pn_ssl_mode_t mode;
  bool allow_unsecured; // allow non-SSL connections
  bool has_ca_db;       // true when CA database configured
  bool has_certificate; // true when certificate configured
  char *keyfile_pw;
  pn_ssl_verify_mode_t verify_mode;
  char *trusted_CAs;

  char *peer_hostname;
  char *peer_match_pattern;
  pn_ssl_match_flag     match_type;

  pn_transport_t *transport;

  BIO *bio_ssl;         // i/o from/to SSL socket layer
  BIO *bio_ssl_io;      // SSL "half" of network-facing BIO
  BIO *bio_net_io;      // socket-side "half" of network-facing BIO
  bool ssl_shutdown;    // BIO_ssl_shutdown() called on socket.
  bool ssl_closed;      // shutdown complete, or SSL error
  ssize_t app_input_closed;   // error code returned by upper layer process input
  ssize_t app_output_closed;  // error code returned by upper layer process output

  bool read_blocked;    // SSL blocked until more network data is read
  bool write_blocked;   // SSL blocked until data is written to network

  // buffers for holding I/O from "applications" above SSL
#define APP_BUF_SIZE    (4*1024)
  char outbuf[APP_BUF_SIZE];
  size_t out_count;
  char inbuf[APP_BUF_SIZE];
  size_t in_count;

  // process cleartext i/o "above" the SSL layer
  ssize_t (*process_input)(pn_transport_t *, const char *, size_t);
  ssize_t (*process_output)(pn_transport_t *, char *, size_t);

  pn_trace_t trace;
};

// define two sets of allowable ciphers: those that require authentication, and those
// that do not require authentication (anonymous).  See ciphers(1).
#define CIPHERS_AUTHENTICATE    "ALL:!aNULL:!eNULL:@STRENGTH"
#define CIPHERS_ANONYMOUS       "ALL:aNULL:!eNULL:@STRENGTH"

/* */
static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata);
static ssize_t process_input_ssl( pn_transport_t *transport, const char *input_data, size_t len);
static ssize_t process_output_ssl( pn_transport_t *transport, char *input_data, size_t len);
static ssize_t process_input_cleartext(pn_transport_t *transport, const char *input_data, size_t len);
static ssize_t process_output_cleartext(pn_transport_t *transport, char *buffer, size_t max_len);
static ssize_t process_input_unknown(pn_transport_t *transport, const char *input_data, size_t len);
static ssize_t process_output_unknown(pn_transport_t *transport, char *input_data, size_t len);
static connection_mode_t check_for_ssl_connection( const char *data, size_t len );
static int init_ssl_socket( pn_ssl_t * );


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

// log an error and dump the SSL error stack
static void _log_ssl_error(pn_ssl_t *ssl, const char *fmt, ...)
{
  char buf[128];        // see "man ERR_error_string_n()"
  va_list ap;

  if (fmt) {
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }

  unsigned long err = ERR_get_error();
  while (err) {
    ERR_error_string_n(err, buf, sizeof(buf));
    _log_error("%s\n", buf);
    err = ERR_get_error();
  }
}

static void _log_clear_data(pn_ssl_t *ssl, const char *data, size_t len)
{
  if (PN_TRACE_RAW & ssl->trace) {
    fprintf(stderr, "SSL decrypted data: \"");
    pn_fprint_data( stderr, data, len );
    fprintf(stderr, "\"\n");
  }
}

// unrecoverable SSL failure occured, notify transport and generate error code.
static int ssl_failed(pn_ssl_t *ssl)
{
  ssl->ssl_closed = true;
  ssl->app_input_closed = ssl->app_output_closed = PN_ERR;
  // try to grab the first SSL error to add to the failure log
  char buf[128] = "Unknown error.";
  unsigned long ssl_err = ERR_get_error();
  if (ssl_err) {
    ERR_error_string_n( ssl_err, buf, sizeof(buf) );
  }
  _log_ssl_error(ssl, NULL);    // spit out any remaining errors to the log file
  return pn_error_format( ssl->transport->error, PN_ERR, "SSL Failure: %s", buf );
}

// Certificate chain verification callback: return 1 if verified,
// 0 if remote cannot be verified (fail handshake).
//
static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
  if (!preverify_ok || X509_STORE_CTX_get_error_depth(ctx) != 0)
    // already failed, or not at peer cert in chain
    return preverify_ok;

  X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
  SSL *ssn = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  if (!ssn) {
    _log_error("Error: unexpected error - SSL session info not available for peer verify!\n");
    return 0;  // fail connection
  }

  pn_ssl_t *ssl = (pn_ssl_t *)SSL_get_ex_data(ssn, ssl_ex_data_index);
  if (!ssl) {
    _log_error("Error: unexpected error - SSL context info not available for peer verify!\n");
    return 0;  // fail connection
  }

  if (!ssl->peer_match_pattern) return preverify_ok;

  _log( ssl, "Checking commonName in peer cert against host pattern (%s)\n", ssl->peer_match_pattern);

  X509_NAME *name = X509_get_subject_name(cert);
  int i = -1;
  bool matched = false;
  while (!matched && (i = X509_NAME_get_index_by_NID(name, NID_commonName, i)) >= 0) {
    X509_NAME_ENTRY *ne = X509_NAME_get_entry(name, i);
    ASN1_STRING *name_asn1 = X509_NAME_ENTRY_get_data(ne);
    if (name_asn1) {
      unsigned char *str;
      int len = ASN1_STRING_to_UTF8( &str, name_asn1);
      if (len >= 0) {
        _log( ssl, "commonName from peer cert = '%.*s'\n", len, str );
        switch (ssl->match_type) {
        case PN_SSL_MATCH_EXACT:
          matched = (len == strlen(ssl->peer_match_pattern) &&
                     strncasecmp( (const char *)str, ssl->peer_match_pattern, len ) == 0);
          break;
        case PN_SSL_MATCH_WILDCARD:
          // TBD
          break;
        }
        OPENSSL_free(str);
      }
    }
  }

  if (!matched) {
    _log( ssl, "Error: no commonName matching %s found - peer is invalid.\n",
          ssl->peer_match_pattern);
    preverify_ok = 0;
#ifdef X509_V_ERR_APPLICATION_VERIFICATION
    X509_STORE_CTX_set_error( ctx, X509_V_ERR_APPLICATION_VERIFICATION );
#endif
  } else {
    _log( ssl, "commonName matched - peer is valid.\n" );
  }
  return preverify_ok;
}


// this code was generated using the command:
// "openssl dhparam -C -2 2048"
static DH *get_dh2048()
{
  static unsigned char dh2048_p[]={
    0xAE,0xF7,0xE9,0x66,0x26,0x7A,0xAC,0x0A,0x6F,0x1E,0xCD,0x81,
    0xBD,0x0A,0x10,0x7E,0xFA,0x2C,0xF5,0x2D,0x98,0xD4,0xE7,0xD9,
    0xE4,0x04,0x8B,0x06,0x85,0xF2,0x0B,0xA3,0x90,0x15,0x56,0x0C,
    0x8B,0xBE,0xF8,0x48,0xBB,0x29,0x63,0x75,0x12,0x48,0x9D,0x7E,
    0x7C,0x24,0xB4,0x3A,0x38,0x7E,0x97,0x3C,0x77,0x95,0xB0,0xA2,
    0x72,0xB6,0xE9,0xD8,0xB8,0xFA,0x09,0x1B,0xDC,0xB3,0x80,0x6E,
    0x32,0x0A,0xDA,0xBB,0xE8,0x43,0x88,0x5B,0xAB,0xC3,0xB2,0x44,
    0xE1,0x95,0x85,0x0A,0x0D,0x13,0xE2,0x02,0x1E,0x96,0x44,0xCF,
    0xA0,0xD8,0x46,0x32,0x68,0x63,0x7F,0x68,0xB3,0x37,0x52,0xCE,
    0x3A,0x4E,0x48,0x08,0x7F,0xD5,0x53,0x00,0x59,0xA8,0x2C,0xCB,
    0x51,0x64,0x3D,0x5F,0xEF,0x0E,0x5F,0xE6,0xAF,0xD9,0x1E,0xA2,
    0x35,0x64,0x37,0xD7,0x4C,0xC9,0x24,0xFD,0x2F,0x75,0xBB,0x3A,
    0x15,0x82,0x76,0x4D,0xC2,0x8B,0x1E,0xB9,0x4B,0xA1,0x33,0xCF,
    0xAA,0x3B,0x7C,0xC2,0x50,0x60,0x6F,0x45,0x69,0xD3,0x6B,0x88,
    0x34,0x9B,0xE4,0xF8,0xC6,0xC7,0x5F,0x10,0xA1,0xBA,0x01,0x8C,
    0xDA,0xD1,0xA3,0x59,0x9C,0x97,0xEA,0xC3,0xF6,0x02,0x55,0x5C,
    0x92,0x1A,0x39,0x67,0x17,0xE2,0x9B,0x27,0x8D,0xE8,0x5C,0xE9,
    0xA5,0x94,0xBB,0x7E,0x16,0x6F,0x53,0x5A,0x6D,0xD8,0x03,0xC2,
    0xAC,0x7A,0xCD,0x22,0x98,0x8E,0x33,0x2A,0xDE,0xAB,0x12,0xC0,
    0x0B,0x7C,0x0C,0x20,0x70,0xD9,0x0B,0xAE,0x0B,0x2F,0x20,0x9B,
    0xA4,0xED,0xFD,0x49,0x0B,0xE3,0x4A,0xF6,0x28,0xB3,0x98,0xB0,
    0x23,0x1C,0x09,0x33,
  };
  static unsigned char dh2048_g[]={
    0x02,
  };
  DH *dh;

  if ((dh=DH_new()) == NULL) return(NULL);
  dh->p=BN_bin2bn(dh2048_p,sizeof(dh2048_p),NULL);
  dh->g=BN_bin2bn(dh2048_g,sizeof(dh2048_g),NULL);
  if ((dh->p == NULL) || (dh->g == NULL))
    { DH_free(dh); return(NULL); }
  return(dh);
}


/** Public API - visible to application code */


int pn_ssl_set_credentials( pn_ssl_t *ssl,
                            const char *certificate_file,
                            const char *private_key_file,
                            const char *password)
{
  if (!ssl) return -1;
  if (ssl->ssl) {
    _log_error("Error: attempting to set credentials while SSL in use.\n");
    return -1;
  }

  if (SSL_CTX_use_certificate_chain_file(ssl->ctx, certificate_file) != 1) {
    _log_ssl_error(ssl, "SSL_CTX_use_certificate_chain_file( %s ) failed\n", certificate_file);
    return -3;
  }

  if (password) {
    ssl->keyfile_pw = pn_strdup(password);  // @todo: obfuscate me!!!
    SSL_CTX_set_default_passwd_cb(ssl->ctx, keyfile_pw_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ssl->ctx, ssl->keyfile_pw);
  }

  if (SSL_CTX_use_PrivateKey_file(ssl->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
    _log_ssl_error(ssl, "SSL_CTX_use_PrivateKey_file( %s ) failed\n", private_key_file);
    return -4;
  }

  if (SSL_CTX_check_private_key(ssl->ctx) != 1) {
    _log_ssl_error(ssl, "The key file %s is not consistent with the certificate %s\n",
                   private_key_file, certificate_file);
    return -5;
  }

  ssl->has_certificate = true;

  // bug in older versions of OpenSSL: servers may request client cert even if anonymous
  // cipher was negotiated.  TLSv1 will reject such a request.  Hack: once a cert is
  // configured, allow only authenticated ciphers.
  if (!SSL_CTX_set_cipher_list( ssl->ctx, CIPHERS_AUTHENTICATE )) {
      _log_ssl_error(ssl, "Failed to set cipher list to %s\n", CIPHERS_AUTHENTICATE);
      return -6;
  }

  _log( ssl, "Configured local certificate file %s\n", certificate_file );
  return 0;
}


int pn_ssl_set_trusted_ca_db(pn_ssl_t *ssl,
                             const char *certificate_db)
{
  if (!ssl) return 0;
  if (ssl->ssl) {
    _log_error("Error: attempting to set trusted CA db after SSL connection initialized.\n");
    return -1;
  }

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
    _log_ssl_error(ssl, "SSL_CTX_load_verify_locations( %s ) failed\n", certificate_db);
    return -1;
  }

  ssl->has_ca_db = true;

  _log( ssl, "loaded trusted CA database: file=%s dir=%s\n", file, dir );
  return 0;
}


int pn_ssl_allow_unsecured_client(pn_ssl_t *ssl)
{
  if (ssl) {
    if (ssl->mode != PN_SSL_MODE_SERVER) {
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
  if (ssl->ssl) {
    _log_error("Error: attempting to set peer authentication after SSL connection initialized.\n");
    return -1;
  }

  switch (mode) {
  case PN_SSL_VERIFY_PEER:

    if (!ssl->has_ca_db) {
      _log_error("Error: cannot verify peer without a trusted CA configured.\n"
                 "       Use pn_ssl_set_trusted_ca_db()\n");
      return -1;
    }

    if (ssl->mode == PN_SSL_MODE_SERVER) {
      // openssl requires that server connections supply a list of trusted CAs which is
      // sent to the client
      if (!trusted_CAs) {
        _log_error("Error: a list of trusted CAs must be provided.\n");
        return -1;
      }
      if (!ssl->has_certificate) {
      _log_error("Error: Server cannot verify peer without configuring a certificate.\n"
                 "       Use pn_ssl_set_credentials()\n");
      }

      ssl->trusted_CAs = pn_strdup( trusted_CAs );
      STACK_OF(X509_NAME) *cert_names;
      cert_names = SSL_load_client_CA_file( ssl->trusted_CAs );
      if (cert_names != NULL)
        SSL_CTX_set_client_CA_list(ssl->ctx, cert_names);
      else {
        _log_error("Unable to process file of trusted CAs: %s\n", trusted_CAs);
        return -1;
      }
    }

    SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                        verify_callback);
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(ssl->ctx, 1);
#endif
    _log( ssl, "Peer authentication mode set to VERIFY-PEER\n");
    break;

  case PN_SSL_ANONYMOUS_PEER:   // hippie free love mode... :)
    SSL_CTX_set_verify( ssl->ctx, SSL_VERIFY_NONE, NULL );
    _log( ssl, "Peer authentication mode set to ANONYMOUS-PEER\n");
    break;

  default:
    _log_error( "Invalid peer authentication mode given.\n" );
    return -1;
  }

  ssl->verify_mode = mode;
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

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size )
{
  const SSL_CIPHER *c;

  *buffer = '\0';
  if (ssl->ssl && (c = SSL_get_current_cipher( ssl->ssl ))) {
    const char *v = SSL_CIPHER_get_name(c);
    if (v) {
      snprintf( buffer, size, "%s", v );
      return true;
    }
  }
  return false;
}

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size )
{
  const SSL_CIPHER *c;

  *buffer = '\0';
  if (ssl->ssl && (c = SSL_get_current_cipher( ssl->ssl ))) {
    const char *v = SSL_CIPHER_get_version(c);
    if (v) {
      snprintf( buffer, size, "%s", v );
      return true;
    }
  }
  return false;
}



int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_mode_t mode)
{
  if (!ssl) return -1;
  if (ssl->mode == mode) return 0;      // already set
  if (ssl->ssl) {
    _log_error("Unable to change mode once SSL is active.\n");
    return -1;
  }

  // if changing the mode from the default, must release old context
  if (ssl->ctx) SSL_CTX_free( ssl->ctx );

  switch (mode) {
  case PN_SSL_MODE_CLIENT:
    _log( ssl, "Setting up Client SSL object.\n" );
    ssl->mode = PN_SSL_MODE_CLIENT;
    ssl->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl->ctx) {
      _log_error("Unable to initialize SSL context: %s\n", strerror(errno));
      return -1;
    }
    break;

  case PN_SSL_MODE_SERVER:
    _log( ssl, "Setting up Server SSL object.\n" );
    ssl->mode = PN_SSL_MODE_SERVER;
    ssl->ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ssl->ctx) {
      _log_error("Unable to initialize SSL context: %s\n", strerror(errno));
      return -1;
    }
    break;

  default:
    _log_error("Invalid valid for pn_ssl_mode_t: %d\n", mode);
    return -1;
  }

  // by default, allow anonymous ciphers so certificates are not required 'out of the box'
  if (!SSL_CTX_set_cipher_list( ssl->ctx, CIPHERS_ANONYMOUS )) {
    _log_ssl_error(ssl, "Failed to set cipher list to %s\n", CIPHERS_ANONYMOUS);
    return -2;
  }

  // ditto: by default do not authenticate the peer (can be done by SASL).
  if (pn_ssl_set_peer_authentication( ssl, PN_SSL_ANONYMOUS_PEER, NULL )) {
    return -2;
  }

  DH *dh = get_dh2048();
  if (dh) {
    SSL_CTX_set_tmp_dh(ssl->ctx, dh);
    DH_free(dh);
    SSL_CTX_set_options(ssl->ctx, SSL_OP_SINGLE_DH_USE);
  }

  return 0;
}

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) return transport->ssl;

  if (!ssl_initialized) {
    ssl_initialized = 1;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ssl_ex_data_index = SSL_get_ex_new_index( 0, "org.apache.qpid.proton.ssl",
                                              NULL, NULL, NULL);
  }

  pn_ssl_t *ssl = calloc(1, sizeof(pn_ssl_t));
  if (!ssl) return NULL;

  ssl->transport = transport;
  ssl->process_input = process_input_ssl;
  ssl->process_output = process_output_ssl;
  transport->ssl = ssl;

  ssl->trace = PN_TRACE_OFF;

  // default mode is client
  if (pn_ssl_init(ssl, PN_SSL_MODE_CLIENT)) {
    free(ssl);
    return NULL;
  }
  return ssl;
}


void pn_ssl_free( pn_ssl_t *ssl)
{
  if (!ssl) return;
  _log( ssl, "SSL socket freed.\n" );
  if (ssl->bio_ssl) BIO_free(ssl->bio_ssl);
  if (ssl->ssl) SSL_free(ssl->ssl);
  else {
    if (ssl->bio_ssl_io) BIO_free(ssl->bio_ssl_io);
    if (ssl->bio_net_io) BIO_free(ssl->bio_net_io);
  }
  if (ssl->ctx) SSL_CTX_free(ssl->ctx);

  if (ssl->keyfile_pw) free(ssl->keyfile_pw);
  if (ssl->trusted_CAs) free(ssl->trusted_CAs);
  if (ssl->peer_hostname) free(ssl->peer_hostname);
  if (ssl->peer_match_pattern) free(ssl->peer_match_pattern);

  free(ssl);
}

// move data received from the network into the SSL layer
ssize_t pn_ssl_input(pn_ssl_t *ssl, const char *bytes, size_t available)
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
    _log(ssl, "Shutting down SSL connection...\n");
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


// take data from the network, and pass it into SSL.  Attempt to read decrypted data from
// SSL socket and pass it to the application.
static ssize_t process_input_ssl( pn_transport_t *transport, const char *input_data, size_t available)
{
  pn_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_ERR;
  if (ssl->ssl == NULL && init_ssl_socket(ssl)) return PN_ERR;

  _log( ssl, "process_input_ssl( data size=%d )\n",available );

  ssize_t consumed = 0;
  bool work_pending;
  bool shutdown_input = (available == 0);  // caller is closed

  do {
    work_pending = false;

    // Write to network bio as much as possible, consuming bytes/available

    if (available > 0) {
      int written = BIO_write( ssl->bio_net_io, input_data, available );
      if (written > 0) {
        input_data += written;
        available -= written;
        consumed += written;
        ssl->read_blocked = false;
        work_pending = (available > 0);
        _log( ssl, "Wrote %d bytes to BIO Layer, %d left over\n", written, available );
      }
    } else if (shutdown_input) {
      // lower layer (caller) has closed.  Close the WRITE side of the BIO.  This will cause
      // an EOF to be passed to SSL once all pending inbound data has been consumed.
      _log( ssl, "Lower layer closed - shutting down BIO write side\n");
      (void)BIO_shutdown_wr( ssl->bio_net_io );
      shutdown_input = false;
    }

    // Read all available data from the SSL socket

    if (!ssl->ssl_closed && ssl->in_count < APP_BUF_SIZE) {
      int read = BIO_read( ssl->bio_ssl, &ssl->inbuf[ssl->in_count], APP_BUF_SIZE - ssl->in_count );
      if (read > 0) {
        _log( ssl, "Read %d bytes from SSL socket for app\n", read );
        _log_clear_data( ssl, &ssl->inbuf[ssl->in_count], read );
        ssl->in_count += read;
        work_pending = true;
      } else {
        if (!BIO_should_retry(ssl->bio_ssl)) {
          int reason = SSL_get_error( ssl->ssl, read );
          switch (reason) {
          case SSL_ERROR_ZERO_RETURN:
            // SSL closed cleanly
            _log(ssl, "SSL connection has closed\n");
            start_ssl_shutdown(ssl);  // KAG: not sure - this may not be necessary
            ssl->ssl_closed = true;
            break;
          default:
            // unexpected error
            return (ssize_t)ssl_failed(ssl);
          }
        } else {
          if (BIO_should_write( ssl->bio_ssl )) {
            ssl->write_blocked = true;
            _log(ssl, "Detected write-blocked\n");
          }
          if (BIO_should_read( ssl->bio_ssl )) {
            ssl->read_blocked = true;
            _log(ssl, "Detected read-blocked\n");
          }
        }
      }
    }

    // write incoming data to app layer

    if (!ssl->app_input_closed) {
      char *data = ssl->inbuf;
      if (ssl->in_count > 0 || ssl->ssl_closed) {  /* if ssl_closed, send 0 count */
        ssize_t consumed = transport->process_input(transport, data, ssl->in_count);
        if (consumed > 0) {
          ssl->in_count -= consumed;
          data += consumed;
          work_pending = true;
          _log( ssl, "Application consumed %d bytes from peer\n", (int) consumed );
        } else {
          if (consumed < 0) {
            _log(ssl, "Application layer closed its input, error=%d (discarding %d bytes)\n",
                 (int) consumed, (int)ssl->in_count);
            ssl->in_count = 0;    // discard any pending input
            ssl->app_input_closed = consumed;
            if (ssl->app_output_closed && ssl->out_count == 0) {
              // both sides of app closed, and no more app output pending:
              start_ssl_shutdown(ssl);
            }
          }
        }
      }
      if (ssl->in_count > 0 && data != ssl->inbuf)
        memmove( ssl->inbuf, data, ssl->in_count );
    }

  } while (work_pending);

  //_log(ssl, "ssl_closed=%d in_count=%d app_input_closed=%d app_output_closed=%d\n",
  //     ssl->ssl_closed, ssl->in_count, ssl->app_input_closed, ssl->app_output_closed );

  // PROTON-82: Instead, close the input side as soon as we've completed enough of the SSL
  // shutdown handshake to send the close_notify.  We're not requiring the response, as
  // some implementations never reply.
  // ---
  // tell transport our input side is closed if the SSL socket cannot be read from any
  // longer, AND any pending input has been written up to the application (or the
  // application is closed)
  //if (ssl->ssl_closed && ssl->app_input_closed) {
  //  consumed = ssl->app_input_closed;
  //}
  if (ssl->app_input_closed && (SSL_get_shutdown(ssl->ssl) & SSL_SENT_SHUTDOWN) ) {
    consumed = ssl->app_input_closed;
  }
  _log(ssl, "process_input_ssl() returning %d\n", (int) consumed);
  return consumed;
}

static ssize_t process_output_ssl( pn_transport_t *transport, char *buffer, size_t max_len)
{
  pn_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_ERR;
  if (ssl->ssl == NULL && init_ssl_socket(ssl)) return PN_ERR;

  ssize_t written = 0;
  bool work_pending;

  do {
    work_pending = false;
    // first, get any pending application output, if possible

    if (!ssl->app_output_closed && ssl->out_count < APP_BUF_SIZE) {
      ssize_t app_bytes = transport->process_output(transport, &ssl->outbuf[ssl->out_count], APP_BUF_SIZE - ssl->out_count);
      if (app_bytes > 0) {
        ssl->out_count += app_bytes;
        work_pending = true;
        _log( ssl, "Gathered %d bytes from app to send to peer\n", app_bytes );
      } else {
        if (app_bytes < 0) {
          _log(ssl, "Application layer closed its output, error=%d (%d bytes pending send)\n",
               (int) app_bytes, (int) ssl->out_count);
          ssl->app_output_closed = app_bytes;
        }
      }
    }

    // now push any pending app data into the socket

    if (!ssl->ssl_closed) {
      char *data = ssl->outbuf;
      if (ssl->out_count > 0) {
        int wrote = BIO_write( ssl->bio_ssl, data, ssl->out_count );
        if (wrote > 0) {
          data += wrote;
          ssl->out_count -= wrote;
          work_pending = true;
          _log( ssl, "Wrote %d bytes from app to socket\n", wrote );
        } else {
          if (!BIO_should_retry(ssl->bio_ssl)) {
            int reason = SSL_get_error( ssl->ssl, wrote );
            switch (reason) {
            case SSL_ERROR_ZERO_RETURN:
              // SSL closed cleanly
              _log(ssl, "SSL connection has closed\n");
              start_ssl_shutdown(ssl); // KAG: not sure - this may not be necessary
              ssl->out_count = 0;      // can no longer write to socket, so erase app output data
              ssl->ssl_closed = true;
              break;
            default:
              // unexpected error
              return (ssize_t)ssl_failed(ssl);
            }
          } else {
            if (BIO_should_read( ssl->bio_ssl )) {
              ssl->read_blocked = true;
              _log(ssl, "Detected read-blocked\n");
            }
            if (BIO_should_write( ssl->bio_ssl )) {
              ssl->write_blocked = true;
              _log(ssl, "Detected write-blocked\n");
            }
          }
        }
      }

      if (ssl->out_count == 0) {
        if (ssl->app_input_closed && ssl->app_output_closed) {
          // application is done sending/receiving data, and all buffered output data has
          // been written to the SSL socket
          start_ssl_shutdown(ssl);
        }
      } else if (data != ssl->outbuf) {
        memmove( ssl->outbuf, data, ssl->out_count );
      }
    }

    // read from the network bio as much as possible, filling the buffer
    if (max_len) {
      int available = BIO_read( ssl->bio_net_io, buffer, max_len );
      if (available > 0) {
        max_len -= available;
        buffer += available;
        written += available;
        ssl->write_blocked = false;
        work_pending = work_pending || max_len > 0;
        _log( ssl, "Read %d bytes from BIO Layer\n", available );
      }
    }

  } while (work_pending);

  //_log(ssl, "written=%d ssl_closed=%d in_count=%d app_input_closed=%d app_output_closed=%d bio_pend=%d\n",
  //     written, ssl->ssl_closed, ssl->in_count, ssl->app_input_closed, ssl->app_output_closed, BIO_pending(ssl->bio_net_io) );

  // PROTON-82: close the output side as soon as we've sent the SSL close_notify.
  // We're not requiring the response, as some implementations never reply.
  // ----
  // Once no more data is available "below" the SSL socket, tell the transport we are
  // done.
  //if (written == 0 && ssl->ssl_closed && BIO_pending(ssl->bio_net_io) == 0) {
  //  written = ssl->app_output_closed ? ssl->app_output_closed : PN_EOS;
  //}
  if (written == 0 && (SSL_get_shutdown(ssl->ssl) & SSL_SENT_SHUTDOWN) && BIO_pending(ssl->bio_net_io) == 0) {
    written = ssl->app_output_closed ? ssl->app_output_closed : PN_EOS;
  }
  _log(ssl, "process_output_ssl() returning %d\n", (int) written);
  return written;
}

static int init_ssl_socket( pn_ssl_t *ssl )
{
  if (ssl->ssl) return 0;

  ssl->ssl = SSL_new(ssl->ctx);
  if (!ssl->ssl) {
    _log_error( "SSL socket setup failure.\n" );
    return -1;
  }

  // store backpointer to pn_ssl_t in SSL object:
  SSL_set_ex_data(ssl->ssl, ssl_ex_data_index, ssl);

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
  if (ssl->peer_hostname) {
    SSL_set_tlsext_host_name(ssl->ssl, ssl->peer_hostname);
  }
#endif

  // now layer a BIO over the SSL socket
  ssl->bio_ssl = BIO_new(BIO_f_ssl());
  if (!ssl->bio_ssl) {
    _log_error( "BIO setup failure.\n" );
    return -1;
  }
  (void)BIO_set_ssl(ssl->bio_ssl, ssl->ssl, BIO_NOCLOSE);

  // create the "lower" BIO "pipe", and attach it below the SSL layer
  if (!BIO_new_bio_pair(&ssl->bio_ssl_io, 0, &ssl->bio_net_io, 0)) {
    _log_error( "BIO setup failure.\n" );
    return -1;
  }
  SSL_set_bio(ssl->ssl, ssl->bio_ssl_io, ssl->bio_ssl_io);

  if (ssl->mode == PN_SSL_MODE_SERVER) {
    SSL_set_accept_state(ssl->ssl);
    BIO_set_ssl_mode(ssl->bio_ssl, 0);  // server mode
    _log( ssl, "Server SSL socket created.\n" );
  } else {      // client mode
    SSL_set_connect_state(ssl->ssl);
    BIO_set_ssl_mode(ssl->bio_ssl, 1);  // client mode
    _log( ssl, "Client SSL socket created.\n" );
  }
  return 0;
}

//////// CLEARTEXT CONNECTIONS

static ssize_t process_input_cleartext(pn_transport_t *transport, const char *input_data, size_t len)
{
  // just write directly to layer "above" SSL
  return transport->process_input( transport, input_data, len );
}

static ssize_t process_output_cleartext(pn_transport_t *transport, char *buffer, size_t max_len)
{
  // just read directly from the layer "above" SSL
  return transport->process_output( transport, buffer, max_len );
}



static int setup_cleartext_connection( pn_ssl_t *ssl )
{
  _log( ssl, "Cleartext connection detected.\n");
  ssl->process_input = process_input_cleartext;
  ssl->process_output = process_output_cleartext;
  return 0;
}


// until we determine if the client is using SSL or not:

static ssize_t process_input_unknown(pn_transport_t *transport, const char *input_data, size_t len)
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

void pn_ssl_set_peer_hostname( pn_ssl_t *ssl, const char *hostname)
{
  if (!ssl) return;

  if (ssl->peer_hostname) free(ssl->peer_hostname);
  ssl->peer_hostname = NULL;
  if (hostname) {
    ssl->peer_hostname = pn_strdup(hostname);
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    if (ssl->ssl) {
      SSL_set_tlsext_host_name(ssl->ssl, ssl->peer_hostname);
    }
#endif
    if (!ssl->peer_match_pattern)
      pn_ssl_set_peer_hostname_match( ssl, hostname, PN_SSL_MATCH_EXACT );
  }
}

int pn_ssl_set_peer_hostname_match( pn_ssl_t *ssl, const char *pattern, pn_ssl_match_flag flag)
{
  if (!ssl) return -1;

  if (flag != PN_SSL_MATCH_EXACT) return -1;  // @todo support for wildcard

  if (ssl->peer_match_pattern) free(ssl->peer_match_pattern);
  ssl->peer_match_pattern = NULL;
  if (pattern) {
    ssl->peer_match_pattern = strdup(pattern);
    ssl->match_type = flag;
  }
  return 0;
}
