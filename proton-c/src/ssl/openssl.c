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
#include <proton/engine.h>
#include "engine/engine-internal.h"
#include "platform.h"
#include "util.h"

// openssl on windows expects the user to have already included
// winsock.h

#ifdef _MSC_VER
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#if _WIN32_WINNT < 0x0501
#error "Proton requires Windows API support for XP or later."
#endif
#include <winsock2.h>
#include <mswsock.h>
#include <Ws2tcpip.h>
#endif


#include <openssl/ssl.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
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

typedef struct pn_ssl_session_t pn_ssl_session_t;

struct pn_ssl_domain_t {

  SSL_CTX       *ctx;

  char *keyfile_pw;

  // settings used for all connections
  char *trusted_CAs;

  // session cache
  pn_ssl_session_t *ssn_cache_head;
  pn_ssl_session_t *ssn_cache_tail;

  int   ref_count;
  pn_ssl_mode_t mode;
  pn_ssl_verify_mode_t verify_mode;

  bool has_ca_db;       // true when CA database configured
  bool has_certificate; // true when certificate configured
  bool allow_unsecured;
};


struct pni_ssl_t {
  pn_ssl_domain_t  *domain;
  const char    *session_id;
  const char *peer_hostname;
  SSL *ssl;

  BIO *bio_ssl;         // i/o from/to SSL socket layer
  BIO *bio_ssl_io;      // SSL "half" of network-facing BIO
  BIO *bio_net_io;      // socket-side "half" of network-facing BIO
  // buffers for holding I/O from "applications" above SSL
#define APP_BUF_SIZE    (4*1024)
  char *outbuf;
  char *inbuf;

  ssize_t app_input_closed;   // error code returned by upper layer process input
  ssize_t app_output_closed;  // error code returned by upper layer process output

  size_t out_size;
  size_t out_count;
  size_t in_size;
  size_t in_count;

  bool ssl_shutdown;    // BIO_ssl_shutdown() called on socket.
  bool ssl_closed;      // shutdown complete, or SSL error
  bool read_blocked;    // SSL blocked until more network data is read
  bool write_blocked;   // SSL blocked until data is written to network

  char *subject;
  X509 *peer_certificate;
};

static inline pn_transport_t *get_transport_internal(pn_ssl_t *ssl)
{
    // The external pn_sasl_t is really a pointer to the internal pni_transport_t
    return ((pn_transport_t *)ssl);
}

static inline pni_ssl_t *get_ssl_internal(pn_ssl_t *ssl)
{
    // The external pn_sasl_t is really a pointer to the internal pni_transport_t
    return ssl ? ((pn_transport_t *)ssl)->ssl : NULL;
}

struct pn_ssl_session_t {
  const char       *id;
  SSL_SESSION      *session;
  pn_ssl_session_t *ssn_cache_next;
  pn_ssl_session_t *ssn_cache_prev;
};


// define two sets of allowable ciphers: those that require authentication, and those
// that do not require authentication (anonymous).  See ciphers(1).
#define CIPHERS_AUTHENTICATE    "ALL:!aNULL:!eNULL:@STRENGTH"
#define CIPHERS_ANONYMOUS       "ALL:aNULL:!eNULL:@STRENGTH"

/* */
static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata);
static void handle_error_ssl( pn_transport_t *transport, unsigned int layer);
static ssize_t process_input_ssl( pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len);
static ssize_t process_output_ssl( pn_transport_t *transport, unsigned int layer, char *input_data, size_t len);
static ssize_t process_input_done(pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len);
static ssize_t process_output_done(pn_transport_t *transport, unsigned int layer, char *input_data, size_t len);
static int init_ssl_socket(pn_transport_t *, pni_ssl_t *);
static void release_ssl_socket( pni_ssl_t * );
static pn_ssl_session_t *ssn_cache_find( pn_ssl_domain_t *, const char * );
static void ssl_session_free( pn_ssl_session_t *);
static size_t buffered_output( pn_transport_t *transport );
static X509 *get_peer_certificate(pni_ssl_t *ssl);

static void ssl_vlog(pn_transport_t *transport, const char *fmt, va_list ap)
{
  if (transport) {
    if (PN_TRACE_DRV & transport->trace) {
      pn_transport_vlogf(transport, fmt, ap);
    }
  } else {
    pn_transport_vlogf(transport, fmt, ap);
  }
}

static void ssl_log(pn_transport_t *transport, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  ssl_vlog(transport, fmt, ap);
  va_end(ap);
}

static void ssl_log_flush(pn_transport_t* transport)
{
  char buf[128];        // see "man ERR_error_string_n()"
  unsigned long err = ERR_get_error();
  while (err) {
    ERR_error_string_n(err, buf, sizeof(buf));
    ssl_log(transport, "%s", buf);
    err = ERR_get_error();
  }
}

// log an error and dump the SSL error stack
static void ssl_log_error(const char *fmt, ...)
{
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    ssl_vlog(NULL, fmt, ap);
    va_end(ap);
  }

  ssl_log_flush(NULL);
}

static void ssl_log_clear_data(pn_transport_t *transport, const char *data, size_t len)
{
  if (PN_TRACE_RAW & transport->trace) {
    fprintf(stderr, "SSL decrypted data: \"");
    pn_fprint_data( stderr, data, len );
    fprintf(stderr, "\"\n");
  }
}

// unrecoverable SSL failure occured, notify transport and generate error code.
static int ssl_failed(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  SSL_set_shutdown(ssl->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
  ssl->ssl_closed = true;
  ssl->app_input_closed = ssl->app_output_closed = PN_EOS;
  // fake a shutdown so the i/o processing code will close properly
  SSL_set_shutdown(ssl->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
  // try to grab the first SSL error to add to the failure log
  char buf[128] = "Unknown error.";
  unsigned long ssl_err = ERR_get_error();
  if (ssl_err) {
    ERR_error_string_n( ssl_err, buf, sizeof(buf) );
  }
  ssl_log_flush(transport);    // spit out any remaining errors to the log file
  pn_do_error(transport, "amqp:connection:framing-error", "SSL Failure: %s", buf);
  return PN_EOS;
}

/* match the DNS name pattern from the peer certificate against our configured peer
   hostname */
static bool match_dns_pattern( const char *hostname,
                               const char *pattern, int plen )
{
  int slen = (int) strlen(hostname);
  if (memchr( pattern, '*', plen ) == NULL)
    return (plen == slen &&
            pn_strncasecmp( pattern, hostname, plen ) == 0);

  /* dns wildcarded pattern - RFC2818 */
  char plabel[64];   /* max label length < 63 - RFC1034 */
  char slabel[64];

  while (plen > 0 && slen > 0) {
    const char *cptr;
    int len;

    cptr = (const char *) memchr( pattern, '.', plen );
    len = (cptr) ? cptr - pattern : plen;
    if (len > (int) sizeof(plabel) - 1) return false;
    memcpy( plabel, pattern, len );
    plabel[len] = 0;
    if (cptr) ++len;    // skip matching '.'
    pattern += len;
    plen -= len;

    cptr = (const char *) memchr( hostname, '.', slen );
    len = (cptr) ? cptr - hostname : slen;
    if (len > (int) sizeof(slabel) - 1) return false;
    memcpy( slabel, hostname, len );
    slabel[len] = 0;
    if (cptr) ++len;    // skip matching '.'
    hostname += len;
    slen -= len;

    char *star = strchr( plabel, '*' );
    if (!star) {
      if (pn_strcasecmp( plabel, slabel )) return false;
    } else {
      *star = '\0';
      char *prefix = plabel;
      int prefix_len = strlen(prefix);
      char *suffix = star + 1;
      int suffix_len = strlen(suffix);
      if (prefix_len && pn_strncasecmp( prefix, slabel, prefix_len )) return false;
      if (suffix_len && pn_strncasecmp( suffix,
                                     slabel + (strlen(slabel) - suffix_len),
                                     suffix_len )) return false;
    }
  }

  return plen == slen;
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
  SSL *ssn = (SSL *) X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  if (!ssn) {
    pn_transport_logf(NULL, "Error: unexpected error - SSL session info not available for peer verify!");
    return 0;  // fail connection
  }

  pn_transport_t *transport = (pn_transport_t *)SSL_get_ex_data(ssn, ssl_ex_data_index);
  if (!transport) {
    pn_transport_logf(NULL, "Error: unexpected error - SSL context info not available for peer verify!");
    return 0;  // fail connection
  }

  pni_ssl_t *ssl = transport->ssl;
  if (ssl->domain->verify_mode != PN_SSL_VERIFY_PEER_NAME) return preverify_ok;
  if (!ssl->peer_hostname) {
    pn_transport_logf(transport, "Error: configuration error: PN_SSL_VERIFY_PEER_NAME configured, but no peer hostname set!");
    return 0;  // fail connection
  }

  ssl_log(transport, "Checking identifying name in peer cert against '%s'", ssl->peer_hostname);

  bool matched = false;

  /* first check any SubjectAltName entries, as per RFC2818 */
  GENERAL_NAMES *sans = (GENERAL_NAMES *) X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
  if (sans) {
    int name_ct = sk_GENERAL_NAME_num( sans );
    int i;
    for (i = 0; !matched && i < name_ct; ++i) {
      GENERAL_NAME *name = sk_GENERAL_NAME_value( sans, i );
      if (name->type == GEN_DNS) {
        ASN1_STRING *asn1 = name->d.dNSName;
        if (asn1 && asn1->data && asn1->length) {
          unsigned char *str;
          int len = ASN1_STRING_to_UTF8( &str, asn1 );
          if (len >= 0) {
            ssl_log(transport, "SubjectAltName (dns) from peer cert = '%.*s'", len, str );
            matched = match_dns_pattern( ssl->peer_hostname, (const char *)str, len );
            OPENSSL_free( str );
          }
        }
      }
    }
    GENERAL_NAMES_free( sans );
  }

  /* if no general names match, try the CommonName from the subject */
  X509_NAME *name = X509_get_subject_name(cert);
  int i = -1;
  while (!matched && (i = X509_NAME_get_index_by_NID(name, NID_commonName, i)) >= 0) {
    X509_NAME_ENTRY *ne = X509_NAME_get_entry(name, i);
    ASN1_STRING *name_asn1 = X509_NAME_ENTRY_get_data(ne);
    if (name_asn1) {
      unsigned char *str;
      int len = ASN1_STRING_to_UTF8( &str, name_asn1);
      if (len >= 0) {
        ssl_log(transport, "commonName from peer cert = '%.*s'", len, str);
        matched = match_dns_pattern( ssl->peer_hostname, (const char *)str, len );
        OPENSSL_free(str);
      }
    }
  }

  if (!matched) {
    ssl_log(transport, "Error: no name matching %s found in peer cert - rejecting handshake.",
          ssl->peer_hostname);
    preverify_ok = 0;
#ifdef X509_V_ERR_APPLICATION_VERIFICATION
    X509_STORE_CTX_set_error( ctx, X509_V_ERR_APPLICATION_VERIFICATION );
#endif
  } else {
    ssl_log(transport, "Name from peer cert matched - peer is valid.");
  }
  return preverify_ok;
}


// this code was generated using the command:
// "openssl dhparam -C -2 2048"
static DH *get_dh2048(void)
{
  static const unsigned char dh2048_p[]={
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
  static const unsigned char dh2048_g[]={
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

static pn_ssl_session_t *ssn_cache_find( pn_ssl_domain_t *domain, const char *id )
{
  pn_timestamp_t now_msec = pn_i_now();
  long now_sec = (long)(now_msec / 1000);
  pn_ssl_session_t *ssn = LL_HEAD( domain, ssn_cache );
  while (ssn) {
    long expire = SSL_SESSION_get_time( ssn->session )
      + SSL_SESSION_get_timeout( ssn->session );
    if (expire < now_sec) {
      pn_ssl_session_t *next = ssn->ssn_cache_next;
      LL_REMOVE( domain, ssn_cache, ssn );
      ssl_session_free( ssn );
      ssn = next;
      continue;
    }

    if (!strcmp(ssn->id, id)) {
      break;
    }
    ssn = ssn->ssn_cache_next;
  }
  return ssn;
}

static void ssl_session_free( pn_ssl_session_t *ssn)
{
  if (ssn) {
    if (ssn->id) free( (void *)ssn->id );
    if (ssn->session) SSL_SESSION_free( ssn->session );
    free( ssn );
  }
}


/** Public API - visible to application code */

bool pn_ssl_present(void)
{
  return true;
}

pn_ssl_domain_t *pn_ssl_domain( pn_ssl_mode_t mode )
{
  if (!ssl_initialized) {
    ssl_initialized = 1;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ssl_ex_data_index = SSL_get_ex_new_index( 0, (void *) "org.apache.qpid.proton.ssl",
                                              NULL, NULL, NULL);
  }

  pn_ssl_domain_t *domain = (pn_ssl_domain_t *) calloc(1, sizeof(pn_ssl_domain_t));
  if (!domain) return NULL;

  domain->ref_count = 1;
  domain->mode = mode;

  // enable all supported protocol versions, then explicitly disable the
  // known vulnerable ones.  This should allow us to use the latest version
  // of the TLS standard that the installed library supports.
  switch(mode) {
  case PN_SSL_MODE_CLIENT:
    domain->ctx = SSL_CTX_new(SSLv23_client_method()); // and TLSv1+
    if (!domain->ctx) {
      ssl_log_error("Unable to initialize OpenSSL context.");
      free(domain);
      return NULL;
    }
    break;

  case PN_SSL_MODE_SERVER:
    domain->ctx = SSL_CTX_new(SSLv23_server_method()); // and TLSv1+
    if (!domain->ctx) {
      ssl_log_error("Unable to initialize OpenSSL context.");
      free(domain);
      return NULL;
    }
    break;

  default:
    pn_transport_logf(NULL, "Invalid value for pn_ssl_mode_t: %d", mode);
    free(domain);
    return NULL;
  }
  const long reject_insecure = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
  SSL_CTX_set_options(domain->ctx, reject_insecure);
#ifdef SSL_OP_NO_COMPRESSION
  // Mitigate the CRIME vulnerability
  SSL_CTX_set_options(domain->ctx, SSL_OP_NO_COMPRESSION);
#endif

  // by default, allow anonymous ciphers so certificates are not required 'out of the box'
  if (!SSL_CTX_set_cipher_list( domain->ctx, CIPHERS_ANONYMOUS )) {
    ssl_log_error("Failed to set cipher list to %s", CIPHERS_ANONYMOUS);
    pn_ssl_domain_free(domain);
    return NULL;
  }

  // ditto: by default do not authenticate the peer (can be done by SASL).
  if (pn_ssl_domain_set_peer_authentication( domain, PN_SSL_ANONYMOUS_PEER, NULL )) {
    pn_ssl_domain_free(domain);
    return NULL;
  }

  DH *dh = get_dh2048();
  if (dh) {
    SSL_CTX_set_tmp_dh(domain->ctx, dh);
    DH_free(dh);
    SSL_CTX_set_options(domain->ctx, SSL_OP_SINGLE_DH_USE);
  }

  return domain;
}

void pn_ssl_domain_free( pn_ssl_domain_t *domain )
{
  if (--domain->ref_count == 0) {

    pn_ssl_session_t *ssn = LL_HEAD( domain, ssn_cache );
    while (ssn) {
      pn_ssl_session_t *next = ssn->ssn_cache_next;
      LL_REMOVE( domain, ssn_cache, ssn );
      ssl_session_free( ssn );
      ssn = next;
    }

    if (domain->ctx) SSL_CTX_free(domain->ctx);
    if (domain->keyfile_pw) free(domain->keyfile_pw);
    if (domain->trusted_CAs) free(domain->trusted_CAs);
    free(domain);
  }
}


int pn_ssl_domain_set_credentials( pn_ssl_domain_t *domain,
                               const char *certificate_file,
                               const char *private_key_file,
                               const char *password)
{
  if (!domain || !domain->ctx) return -1;

  if (SSL_CTX_use_certificate_chain_file(domain->ctx, certificate_file) != 1) {
    ssl_log_error("SSL_CTX_use_certificate_chain_file( %s ) failed", certificate_file);
    return -3;
  }

  if (password) {
    domain->keyfile_pw = pn_strdup(password);  // @todo: obfuscate me!!!
    SSL_CTX_set_default_passwd_cb(domain->ctx, keyfile_pw_cb);
    SSL_CTX_set_default_passwd_cb_userdata(domain->ctx, domain->keyfile_pw);
  }

  if (SSL_CTX_use_PrivateKey_file(domain->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
    ssl_log_error("SSL_CTX_use_PrivateKey_file( %s ) failed", private_key_file);
    return -4;
  }

  if (SSL_CTX_check_private_key(domain->ctx) != 1) {
    ssl_log_error("The key file %s is not consistent with the certificate %s",
                   private_key_file, certificate_file);
    return -5;
  }

  domain->has_certificate = true;

  // bug in older versions of OpenSSL: servers may request client cert even if anonymous
  // cipher was negotiated.  TLSv1 will reject such a request.  Hack: once a cert is
  // configured, allow only authenticated ciphers.
  if (!SSL_CTX_set_cipher_list( domain->ctx, CIPHERS_AUTHENTICATE )) {
      ssl_log_error("Failed to set cipher list to %s", CIPHERS_AUTHENTICATE);
      return -6;
  }

  return 0;
}


int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                    const char *certificate_db)
{
  if (!domain) return -1;

  // certificates can be either a file or a directory, which determines how it is passed
  // to SSL_CTX_load_verify_locations()
  struct stat sbuf;
  if (stat( certificate_db, &sbuf ) != 0) {
    pn_transport_logf(NULL, "stat(%s) failed: %s", certificate_db, strerror(errno));
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

  if (SSL_CTX_load_verify_locations( domain->ctx, file, dir ) != 1) {
    ssl_log_error("SSL_CTX_load_verify_locations( %s ) failed", certificate_db);
    return -1;
  }

  domain->has_ca_db = true;

  return 0;
}


int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                          const pn_ssl_verify_mode_t mode,
                                          const char *trusted_CAs)
{
  if (!domain) return -1;

  switch (mode) {
  case PN_SSL_VERIFY_PEER:
  case PN_SSL_VERIFY_PEER_NAME:

    if (!domain->has_ca_db) {
      pn_transport_logf(NULL, "Error: cannot verify peer without a trusted CA configured.\n"
                 "       Use pn_ssl_domain_set_trusted_ca_db()");
      return -1;
    }

    if (domain->mode == PN_SSL_MODE_SERVER) {
      // openssl requires that server connections supply a list of trusted CAs which is
      // sent to the client
      if (!trusted_CAs) {
        pn_transport_logf(NULL, "Error: a list of trusted CAs must be provided.");
        return -1;
      }
      if (!domain->has_certificate) {
        pn_transport_logf(NULL, "Error: Server cannot verify peer without configuring a certificate.\n"
                   "       Use pn_ssl_domain_set_credentials()");
      }

      if (domain->trusted_CAs) free(domain->trusted_CAs);
      domain->trusted_CAs = pn_strdup( trusted_CAs );
      STACK_OF(X509_NAME) *cert_names;
      cert_names = SSL_load_client_CA_file( domain->trusted_CAs );
      if (cert_names != NULL)
        SSL_CTX_set_client_CA_list(domain->ctx, cert_names);
      else {
        pn_transport_logf(NULL, "Error: Unable to process file of trusted CAs: %s", trusted_CAs);
        return -1;
      }
    }

    SSL_CTX_set_verify( domain->ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                        verify_callback);
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(domain->ctx, 1);
#endif
    break;

  case PN_SSL_ANONYMOUS_PEER:   // hippie free love mode... :)
    SSL_CTX_set_verify( domain->ctx, SSL_VERIFY_NONE, NULL );
    break;

  default:
    pn_transport_logf(NULL, "Invalid peer authentication mode given." );
    return -1;
  }

  domain->verify_mode = mode;
  return 0;
}

const pn_io_layer_t ssl_layer = {
    process_input_ssl,
    process_output_ssl,
    handle_error_ssl,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_input_closed_layer = {
    process_input_done,
    process_output_ssl,
    handle_error_ssl,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_output_closed_layer = {
    process_input_ssl,
    process_output_done,
    handle_error_ssl,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_closed_layer = {
    process_input_done,
    process_output_done,
    handle_error_ssl,
    NULL,
    buffered_output
};

int pn_ssl_init(pn_ssl_t *ssl0, pn_ssl_domain_t *domain, const char *session_id)
{
  pn_transport_t *transport = get_transport_internal(ssl0);
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl || !domain || ssl->domain) return -1;

  ssl->domain = domain;
  domain->ref_count++;
  if (session_id && domain->mode == PN_SSL_MODE_CLIENT)
    ssl->session_id = pn_strdup(session_id);

  // If SSL doesn't specifically allow skipping encryption, require SSL
  // TODO: This is a probably a stop-gap until allow_unsecured is removed
  if (!domain->allow_unsecured) transport->encryption_required = true;

  return init_ssl_socket(transport, ssl);
}


int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain)
{
  if (!domain) return -1;
  if (domain->mode != PN_SSL_MODE_SERVER) {
    pn_transport_logf(NULL, "Cannot permit unsecured clients - not a server.");
    return -1;
  }
  domain->allow_unsecured = true;
  return 0;
}

int pn_ssl_get_ssf(pn_ssl_t *ssl0)
{
  const SSL_CIPHER *c;

  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (ssl && ssl->ssl && (c = SSL_get_current_cipher( ssl->ssl ))) {
    return SSL_CIPHER_get_bits(c, NULL);
  }
  return 0;
}

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl0, char *buffer, size_t size )
{
  const SSL_CIPHER *c;

  pni_ssl_t *ssl = get_ssl_internal(ssl0);
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

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl0, char *buffer, size_t size )
{
  const SSL_CIPHER *c;

  pni_ssl_t *ssl = get_ssl_internal(ssl0);
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


void pn_ssl_free(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl) return;
  ssl_log(transport, "SSL socket freed." );
  release_ssl_socket( ssl );
  if (ssl->domain) pn_ssl_domain_free(ssl->domain);
  if (ssl->session_id) free((void *)ssl->session_id);
  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  if (ssl->inbuf) free((void *)ssl->inbuf);
  if (ssl->outbuf) free((void *)ssl->outbuf);
  if (ssl->subject) free(ssl->subject);
  if (ssl->peer_certificate) X509_free(ssl->peer_certificate);
  free(ssl);
}

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) return (pn_ssl_t *) transport;

  pni_ssl_t *ssl = (pni_ssl_t *) calloc(1, sizeof(pni_ssl_t));
  if (!ssl) return NULL;
  ssl->out_size = APP_BUF_SIZE;
  uint32_t max_frame = pn_transport_get_max_frame(transport);
  ssl->in_size =  max_frame ? max_frame : APP_BUF_SIZE;
  ssl->outbuf = (char *)malloc(ssl->out_size);
  if (!ssl->outbuf) {
    free(ssl);
    return NULL;
  }
  ssl->inbuf =  (char *)malloc(ssl->in_size);
  if (!ssl->inbuf) {
    free(ssl->outbuf);
    free(ssl);
    return NULL;
  }

  transport->ssl = ssl;

  // Set up hostname from any bound connection
  if (transport->connection) {
    if (pn_string_size(transport->connection->hostname)) {
      pn_ssl_set_peer_hostname((pn_ssl_t *) transport, pn_string_get(transport->connection->hostname));
    }
  }

  return (pn_ssl_t *) transport;
}


/** Private: */

static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata)
{
    strncpy(buf, (char *)userdata, size);   // @todo: un-obfuscate me!!!
    buf[size - 1] = '\0';
    return(strlen(buf));
}


static int start_ssl_shutdown(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl->ssl_shutdown) {
    ssl_log(transport, "Shutting down SSL connection...");
    if (ssl->session_id) {
      // save the negotiated credentials before we close the connection
      pn_ssl_session_t *ssn = (pn_ssl_session_t *)calloc( 1, sizeof(pn_ssl_session_t));
      if (ssn) {
        ssn->id = pn_strdup( ssl->session_id );
        ssn->session = SSL_get1_session( ssl->ssl );
        if (ssn->session) {
          ssl_log(transport, "Saving SSL session as %s", ssl->session_id );
          LL_ADD( ssl->domain, ssn_cache, ssn );
        } else {
          ssl_session_free( ssn );
        }
      }
    }
    ssl->ssl_shutdown = true;
    BIO_ssl_shutdown( ssl->bio_ssl );
  }
  return 0;
}


//////// SSL Connections


// take data from the network, and pass it into SSL.  Attempt to read decrypted data from
// SSL socket and pass it to the application.
static ssize_t process_input_ssl( pn_transport_t *transport, unsigned int layer, const char *input_data, size_t available)
{
  pni_ssl_t *ssl = transport->ssl;
  if (ssl->ssl == NULL && init_ssl_socket(transport, ssl)) return PN_EOS;

  ssl_log( transport, "process_input_ssl( data size=%d )",available );

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
        ssl_log( transport, "Wrote %d bytes to BIO Layer, %d left over", written, available );
      }
    } else if (shutdown_input) {
      // lower layer (caller) has closed.  Close the WRITE side of the BIO.  This will cause
      // an EOF to be passed to SSL once all pending inbound data has been consumed.
      ssl_log( transport, "Lower layer closed - shutting down BIO write side");
      (void)BIO_shutdown_wr( ssl->bio_net_io );
      shutdown_input = false;
    }

    // Read all available data from the SSL socket

    if (!ssl->ssl_closed && ssl->in_count < ssl->in_size) {
      int read = BIO_read( ssl->bio_ssl, &ssl->inbuf[ssl->in_count], ssl->in_size - ssl->in_count );
      if (read > 0) {
        ssl_log( transport, "Read %d bytes from SSL socket for app", read );
        ssl_log_clear_data(transport, &ssl->inbuf[ssl->in_count], read );
        ssl->in_count += read;
        work_pending = true;
      } else {
        if (!BIO_should_retry(ssl->bio_ssl)) {
          int reason = SSL_get_error( ssl->ssl, read );
          switch (reason) {
          case SSL_ERROR_ZERO_RETURN:
            // SSL closed cleanly
            ssl_log(transport, "SSL connection has closed");
            start_ssl_shutdown(transport);  // KAG: not sure - this may not be necessary
            ssl->ssl_closed = true;
            break;
          default:
            // unexpected error
            return (ssize_t)ssl_failed(transport);
          }
        } else {
          if (BIO_should_write( ssl->bio_ssl )) {
            ssl->write_blocked = true;
            ssl_log(transport, "Detected write-blocked");
          }
          if (BIO_should_read( ssl->bio_ssl )) {
            ssl->read_blocked = true;
            ssl_log(transport, "Detected read-blocked");
          }
        }
      }
    }

    // write incoming data to app layer

    if (!ssl->app_input_closed) {
      if (ssl->in_count > 0 || ssl->ssl_closed) {  /* if ssl_closed, send 0 count */
        ssize_t consumed = transport->io_layers[layer+1]->process_input(transport, layer+1, ssl->inbuf, ssl->in_count);
        if (consumed > 0) {
          ssl->in_count -= consumed;
          if (ssl->in_count)
            memmove( ssl->inbuf, ssl->inbuf + consumed, ssl->in_count );
          work_pending = true;
          ssl_log( transport, "Application consumed %d bytes from peer", (int) consumed );
        } else if (consumed < 0) {
          ssl_log(transport, "Application layer closed its input, error=%d (discarding %d bytes)",
               (int) consumed, (int)ssl->in_count);
          ssl->in_count = 0;    // discard any pending input
          ssl->app_input_closed = consumed;
          if (ssl->app_output_closed && ssl->out_count == 0) {
            // both sides of app closed, and no more app output pending:
            start_ssl_shutdown(transport);
          }
        } else {
          // app did not consume any bytes, must be waiting for a full frame
          if (ssl->in_count == ssl->in_size) {
            // but the buffer is full, not enough room for a full frame.
            // can we grow the buffer?
            uint32_t max_frame = pn_transport_get_max_frame(transport);
            if (!max_frame) max_frame = ssl->in_size * 2;  // no limit
            if (ssl->in_size < max_frame) {
              // no max frame limit - grow it.
              size_t newsize = pn_min(max_frame, ssl->in_size * 2);
              char *newbuf = (char *)realloc( ssl->inbuf, newsize );
              if (newbuf) {
                ssl->in_size = newsize;
                ssl->inbuf = newbuf;
                work_pending = true;  // can we get more input?
              }
            } else {
              // can't gather any more input, but app needs more?
              // This is a bug - since SSL can buffer up to max-frame,
              // the application _must_ have enough data to process.  If
              // this is an oversized frame, the app _must_ handle it
              // by returning an error code to SSL.
              pn_transport_log(transport, "Error: application unable to consume input.");
            }
          }
        }
      }
    }

  } while (work_pending);

  //_log(ssl, "ssl_closed=%d in_count=%d app_input_closed=%d app_output_closed=%d",
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
    if (transport->io_layers[layer]==&ssl_output_closed_layer) {
      transport->io_layers[layer] = &ssl_closed_layer;
    } else {
      transport->io_layers[layer] = &ssl_input_closed_layer;
    }
  }
  ssl_log(transport, "process_input_ssl() returning %d", (int) consumed);
  return consumed;
}

static void handle_error_ssl(pn_transport_t *transport, unsigned int layer)
{
  transport->io_layers[layer] = &ssl_closed_layer;
}

static ssize_t process_output_ssl( pn_transport_t *transport, unsigned int layer, char *buffer, size_t max_len)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_EOS;
  if (ssl->ssl == NULL && init_ssl_socket(transport, ssl)) return PN_EOS;

  ssize_t written = 0;
  bool work_pending;

  do {
    work_pending = false;
    // first, get any pending application output, if possible

    if (!ssl->app_output_closed && ssl->out_count < ssl->out_size) {
      ssize_t app_bytes = transport->io_layers[layer+1]->process_output(transport, layer+1, &ssl->outbuf[ssl->out_count], ssl->out_size - ssl->out_count);
      if (app_bytes > 0) {
        ssl->out_count += app_bytes;
        work_pending = true;
        ssl_log(transport, "Gathered %d bytes from app to send to peer", app_bytes );
      } else {
        if (app_bytes < 0) {
          ssl_log(transport, "Application layer closed its output, error=%d (%d bytes pending send)",
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
          ssl_log( transport, "Wrote %d bytes from app to socket", wrote );
        } else {
          if (!BIO_should_retry(ssl->bio_ssl)) {
            int reason = SSL_get_error( ssl->ssl, wrote );
            switch (reason) {
            case SSL_ERROR_ZERO_RETURN:
              // SSL closed cleanly
              ssl_log(transport, "SSL connection has closed");
              start_ssl_shutdown(transport); // KAG: not sure - this may not be necessary
              ssl->out_count = 0;      // can no longer write to socket, so erase app output data
              ssl->ssl_closed = true;
              break;
            default:
              // unexpected error
              return (ssize_t)ssl_failed(transport);
            }
          } else {
            if (BIO_should_read( ssl->bio_ssl )) {
              ssl->read_blocked = true;
              ssl_log(transport, "Detected read-blocked");
            }
            if (BIO_should_write( ssl->bio_ssl )) {
              ssl->write_blocked = true;
              ssl_log(transport, "Detected write-blocked");
            }
          }
        }
      }

      if (ssl->out_count == 0) {
        if (ssl->app_input_closed && ssl->app_output_closed) {
          // application is done sending/receiving data, and all buffered output data has
          // been written to the SSL socket
          start_ssl_shutdown(transport);
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
        ssl_log(transport, "Read %d bytes from BIO Layer", available );
      }
    }

  } while (work_pending);

  //_log(ssl, "written=%d ssl_closed=%d in_count=%d app_input_closed=%d app_output_closed=%d bio_pend=%d",
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
    if (transport->io_layers[layer]==&ssl_input_closed_layer) {
      transport->io_layers[layer] = &ssl_closed_layer;
    } else {
      transport->io_layers[layer] = &ssl_output_closed_layer;
    }
  }
  ssl_log(transport, "process_output_ssl() returning %d", (int) written);
  return written;
}

static int init_ssl_socket(pn_transport_t* transport, pni_ssl_t *ssl)
{
  if (ssl->ssl) return 0;
  if (!ssl->domain) return -1;

  ssl->ssl = SSL_new(ssl->domain->ctx);
  if (!ssl->ssl) {
    pn_transport_logf(transport, "SSL socket setup failure." );
    return -1;
  }

  // store backpointer to pn_transport_t in SSL object:
  SSL_set_ex_data(ssl->ssl, ssl_ex_data_index, transport);

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
  if (ssl->peer_hostname && ssl->domain->mode == PN_SSL_MODE_CLIENT) {
    SSL_set_tlsext_host_name(ssl->ssl, ssl->peer_hostname);
  }
#endif

  // restore session, if available
  if (ssl->session_id) {
    pn_ssl_session_t *ssn = ssn_cache_find( ssl->domain, ssl->session_id );
    if (ssn) {
      ssl_log( transport, "Restoring previous session id=%s", ssn->id );
      int rc = SSL_set_session( ssl->ssl, ssn->session );
      if (rc != 1) {
        ssl_log( transport, "Session restore failed, id=%s", ssn->id );
      }
      LL_REMOVE( ssl->domain, ssn_cache, ssn );
      ssl_session_free( ssn );
    }
  }

  // now layer a BIO over the SSL socket
  ssl->bio_ssl = BIO_new(BIO_f_ssl());
  if (!ssl->bio_ssl) {
    pn_transport_log(transport, "BIO setup failure." );
    return -1;
  }
  (void)BIO_set_ssl(ssl->bio_ssl, ssl->ssl, BIO_NOCLOSE);

  // create the "lower" BIO "pipe", and attach it below the SSL layer
  if (!BIO_new_bio_pair(&ssl->bio_ssl_io, 0, &ssl->bio_net_io, 0)) {
    pn_transport_log(transport, "BIO setup failure." );
    return -1;
  }
  SSL_set_bio(ssl->ssl, ssl->bio_ssl_io, ssl->bio_ssl_io);

  if (ssl->domain->mode == PN_SSL_MODE_SERVER) {
    SSL_set_accept_state(ssl->ssl);
    BIO_set_ssl_mode(ssl->bio_ssl, 0);  // server mode
    ssl_log( transport, "Server SSL socket created." );
  } else {      // client mode
    SSL_set_connect_state(ssl->ssl);
    BIO_set_ssl_mode(ssl->bio_ssl, 1);  // client mode
    ssl_log( transport, "Client SSL socket created." );
  }
  ssl->subject = NULL;
  ssl->peer_certificate = NULL;
  return 0;
}

static void release_ssl_socket(pni_ssl_t *ssl)
{
  if (ssl->bio_ssl) BIO_free(ssl->bio_ssl);
  if (ssl->ssl) {
      SSL_free(ssl->ssl);       // will free bio_ssl_io
  } else {
    if (ssl->bio_ssl_io) BIO_free(ssl->bio_ssl_io);
  }
  if (ssl->bio_net_io) BIO_free(ssl->bio_net_io);
  ssl->bio_ssl = NULL;
  ssl->bio_ssl_io = NULL;
  ssl->bio_net_io = NULL;
  ssl->ssl = NULL;
}



pn_ssl_resume_status_t pn_ssl_resume_status(pn_ssl_t *ssl0)
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl || !ssl->ssl) return PN_SSL_RESUME_UNKNOWN;
  switch (SSL_session_reused( ssl->ssl )) {
  case 0: return PN_SSL_RESUME_NEW;
  case 1: return PN_SSL_RESUME_REUSED;
  default: break;
  }
  return PN_SSL_RESUME_UNKNOWN;
}


int pn_ssl_set_peer_hostname(pn_ssl_t *ssl0, const char *hostname)
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl) return -1;

  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  ssl->peer_hostname = NULL;
  if (hostname) {
    ssl->peer_hostname = pn_strdup(hostname);
    if (!ssl->peer_hostname) return -2;
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    if (ssl->ssl && ssl->domain && ssl->domain->mode == PN_SSL_MODE_CLIENT) {
      SSL_set_tlsext_host_name(ssl->ssl, ssl->peer_hostname);
    }
#endif
  }
  return 0;
}

int pn_ssl_get_peer_hostname(pn_ssl_t *ssl0, char *hostname, size_t *bufsize)
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl) return -1;
  if (!ssl->peer_hostname) {
    *bufsize = 0;
    if (hostname) *hostname = '\0';
    return 0;
  }
  unsigned len = strlen(ssl->peer_hostname);
  if (hostname) {
    if (len >= *bufsize) return -1;
    strcpy( hostname, ssl->peer_hostname );
  }
  *bufsize = len;
  return 0;
}

static X509 *get_peer_certificate(pni_ssl_t *ssl)
{
  // Cache for multiple use and final X509_free
  if (!ssl->peer_certificate && ssl->ssl) {
    ssl->peer_certificate = SSL_get_peer_certificate(ssl->ssl);
    // May still be NULL depending on timing or type of SSL connection
  }
  return ssl->peer_certificate;
}

const char* pn_ssl_get_remote_subject(pn_ssl_t *ssl0)
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl || !ssl->ssl) return NULL;
  if (!ssl->subject) {
    X509 *cert = get_peer_certificate(ssl);
    if (!cert) return NULL;
    X509_NAME *subject = X509_get_subject_name(cert);
    if (!subject) return NULL;

    BIO *out = BIO_new(BIO_s_mem());
    X509_NAME_print_ex(out, subject, 0, XN_FLAG_RFC2253);
    int len = BIO_number_written(out);
    ssl->subject = (char*) malloc(len+1);
    ssl->subject[len] = 0;
    BIO_read(out, ssl->subject, len);
    BIO_free(out);
  }
  return ssl->subject;
}

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0, char *fingerprint, size_t fingerprint_length, pn_ssl_hash_alg hash_alg)
{
    const char *digest_name = NULL;
    size_t min_required_length;

    // old versions of python expect fingerprint to contain a valid string on
    // return from this function
    fingerprint[0] = 0;

    // Assign the correct digest_name value based on the enum values.
    switch (hash_alg) {
        case PN_SSL_SHA1 :
            min_required_length = 41; // 40 hex characters + 1 '\0' character
            digest_name = "sha1";
            break;
        case PN_SSL_SHA256 :
            min_required_length = 65; // 64 hex characters + 1 '\0' character
            digest_name = "sha256";
            break;
        case PN_SSL_SHA512 :
            min_required_length = 129; // 128 hex characters + 1 '\0' character
            digest_name = "sha512";
            break;
        case PN_SSL_MD5 :
            min_required_length = 33; // 32 hex characters + 1 '\0' character
            digest_name = "md5";
            break;
        default:
            ssl_log_error("Unknown or unhandled hash algorithm %i \n", hash_alg);
            return PN_ERR;

    }

    if(fingerprint_length < min_required_length) {
        ssl_log_error("Insufficient fingerprint_length %i. fingerprint_length must be %i or above for %s digest\n",
            fingerprint_length, min_required_length, digest_name);
        return PN_ERR;
    }

    const EVP_MD  *digest = EVP_get_digestbyname(digest_name);

    pni_ssl_t *ssl = get_ssl_internal(ssl0);

    X509 *cert = get_peer_certificate(ssl);

    if(cert) {
        unsigned int len;

        unsigned char bytes[64]; // sha512 uses 64 octets, we will use that as the maximum.

        if (X509_digest(cert, digest, bytes, &len) != 1) {
            ssl_log_error("Failed to extract X509 digest\n");
            return PN_ERR;
       }

        char *cursor = fingerprint;

        for (size_t i=0; i<len ; i++) {
            cursor +=  snprintf((char *)cursor, fingerprint_length, "%02x", bytes[i]);
            fingerprint_length = fingerprint_length - 2;
        }

        return PN_OK;
    }
    else {
        ssl_log_error("No certificate is available yet \n");
        return PN_ERR;
    }

    return 0;
}


const char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field)
{
    int openssl_field = 0;

    // Assign openssl internal representations of field values to openssl_field
    switch (field) {
        case PN_SSL_CERT_SUBJECT_COUNTRY_NAME :
            openssl_field = NID_countryName;
            break;
        case PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE :
            openssl_field = NID_stateOrProvinceName;
            break;
        case PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY :
            openssl_field = NID_localityName;
            break;
        case PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME :
            openssl_field = NID_organizationName;
            break;
        case PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT :
            openssl_field = NID_organizationalUnitName;
            break;
        case PN_SSL_CERT_SUBJECT_COMMON_NAME :
            openssl_field = NID_commonName;
            break;
        default:
            ssl_log_error("Unknown or unhandled certificate subject subfield %i \n", field);
            return NULL;
    }

    pni_ssl_t *ssl = get_ssl_internal(ssl0);
    X509 *cert = get_peer_certificate(ssl);

    X509_NAME *subject_name = X509_get_subject_name(cert);

    // TODO (gmurthy) - A server side cert subject field can have more than one common name like this - Subject: CN=www.domain1.com, CN=www.domain2.com, see https://bugzilla.mozilla.org/show_bug.cgi?id=380656
    // For now, we will only return the first common name if there is more than one common name in the cert
    int index = X509_NAME_get_index_by_NID(subject_name, openssl_field, -1);

    if (index > -1) {
        X509_NAME_ENTRY *ne = X509_NAME_get_entry(subject_name, index);
        if(ne) {
            ASN1_STRING *name_asn1 = X509_NAME_ENTRY_get_data(ne);
            return (char *) name_asn1->data;
        }
    }

    return NULL;
}

static ssize_t process_input_done(pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len)
{
  return PN_EOS;
}
static ssize_t process_output_done(pn_transport_t *transport, unsigned int layer, char *input_data, size_t len)
{
  return PN_EOS;
}

// return # output bytes sitting in this layer
static size_t buffered_output(pn_transport_t *transport)
{
  size_t count = 0;
  pni_ssl_t *ssl = transport->ssl;
  if (ssl) {
    count += ssl->out_count;
    if (ssl->bio_net_io) { // pick up any bytes waiting for network io
      count += BIO_ctrl_pending(ssl->bio_net_io);
    }
  }
  return count;
}
